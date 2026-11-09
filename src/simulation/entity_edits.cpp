#include "world.h"
#include "tuning.h"
#include "systems/random.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace {
bool same_fire(const Tree& a, const Tree& b)
{
    return a.charred == b.charred && a.ignition_tick == b.ignition_tick &&
        a.burnout_tick == b.burnout_tick && a.dead_ticks == b.dead_ticks && a.decay_ticks == b.decay_ticks;
}
bool contains(const TileMap& map, glm::ivec2 hex)
{
    // Validate user coordinates before TileMap's fast int arithmetic can overflow.
    const glm::i64vec2 storage = glm::i64vec2(hex) + glm::i64vec2(map.storage_origin());
    const auto column = storage.x + storage.y / 2;
    return storage.y >= 0 && storage.y < map.height && column >= 0 && column < map.width;
}
bool integer(const PropertyValue& value, int& out)
{
    const auto* number = std::get_if<std::int64_t>(&value);
    if (!number || *number < std::numeric_limits<int>::min() || *number > std::numeric_limits<int>::max()) return false;
    out = static_cast<int>(*number); return true;
}
bool real(const PropertyValue& value, float& out)
{
    const auto* number = std::get_if<double>(&value);
    if (!number || !std::isfinite(*number) || std::abs(*number) > std::numeric_limits<float>::max()) return false;
    out = static_cast<float>(*number); return true;
}
}

std::uint64_t World::submit_edit(EntityEdit edit)
{
    const auto request = next_edit_request_++;
    // Bound outstanding requests/receipts if a client stops consuming results.
    if (edits_.size() + edit_results_.size() >= 128) return 0;
    edits_.push_back({request, std::move(edit)});
    return request;
}

std::optional<EntityEditResult> World::take_edit_result(std::uint64_t request)
{
    const auto found = std::find_if(edit_results_.begin(), edit_results_.end(),
        [request](const auto& result) { return result.request == request; });
    if (found == edit_results_.end()) return std::nullopt;
    auto result = std::move(*found);
    edit_results_.erase(found);
    return result;
}

void World::flush_edits()
{
    // One FIFO boundary for paused frames and world steps. Browsing never flushes.
    auto pending = std::move(edits_);
    edits_.clear();
    for (const auto& edit : pending) edit_results_.push_back(apply_edit(edit));
}

EntityEditResult World::apply_edit(const PendingEdit& pending)
{
    const auto& edit = pending.edit;
    const auto reject = [&](const std::string& reason) { return EntityEditResult{pending.request, false, reason}; };
    const auto live = inspect_entity(edit.target);
    if (!live) return reject("Entity was deleted or belongs to an old world. Select a live entity.");
    const auto entity = registry().entity(edit.target.entity);
    if (!entity.has<ActorIdentity>()) return reject("This entity has no domain edit adapter.");
    const auto* old_position = entity.try_get<ActorPosition>();
    const auto* old_appearance = entity.try_get<ActorAppearance>();
    ActorPosition position = old_position ? *old_position : ActorPosition{};
    ActorAppearance appearance = old_appearance ? *old_appearance : ActorAppearance{};
    bool selectable = entity.has<Selectable>();
    bool move = false, change_appearance = false, change_selection = false;
    std::set<std::pair<std::uint64_t, std::string>> seen;
    for (const auto& change : edit.properties) {
        if (!seen.emplace(change.component, change.path).second) return reject("Duplicate property in grouped edit.");
        const EntityProperty* field = nullptr;
        for (const auto& component : live->components)
            for (const auto& candidate : component.properties)
                if (candidate.component == change.component && candidate.path == change.path) field = &candidate;
        if (!field || !field->editable) return reject("Property is read-only or unsupported: " + change.path);
        if (field->value != change.expected) return reject("Conflict: " + change.path + " changed. Revert to reload live values.");
        bool valid = false;
        if (change.component == registry().id<ActorPosition>() && old_position) {
            move = true;
            if (change.path == "hex.x") valid = integer(change.value, position.hex.x);
            if (change.path == "hex.y") valid = integer(change.value, position.hex.y);
            if (change.path == "offset.x") valid = real(change.value, position.offset.x);
            if (change.path == "offset.y") valid = real(change.value, position.offset.y);
        } else if (change.component == registry().id<ActorAppearance>() && old_appearance) {
            change_appearance = true;
            if (change.path == "variant") {
                if (const auto* value = std::get_if<std::uint64_t>(&change.value); value && *value < 3) {
                    appearance.variant = static_cast<unsigned>(*value); valid = true;
                }
            } else if (change.path == "kind") {
                if (const auto* value = std::get_if<std::int64_t>(&change.value); value && *value == 0) {
                    appearance.kind = ActorKind::tree; valid = true;
                }
            }
        } else if (change.component == registry().id<Selectable>() && change.path == "selectable") {
            if (const auto* value = std::get_if<bool>(&change.value)) {
                selectable = *value; change_selection = true; valid = true;
            }
        }
        if (!valid) return reject("Invalid value or enum choice: " + change.path);
    }
    if (move) {
        if (!contains(map_, position.hex)) return reject("Position is outside the map.");
        const auto offset = glm::abs(position.offset);
        if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || offset.x > glm::sqrt(3.0f) / 2 ||
            offset.y > 1.0f - offset.x / glm::sqrt(3.0f)) return reject("Ground offset is outside its hexagon.");
        if (live->tree) {
            if (live->tree->burning() || live->tree->charred) return reject("Burning trees and charred remains cannot move.");
            const int cell = map_.index(position.hex);
            if (!simulation_detail::can_grow(map_.tiles[cell])) return reject("Trees require grass or land.");
            const auto occupant = actor_at(cell);
            if (occupant && occupant != edit.target) return reject("Another tree occupies that hex.");
        }
    }
    Tree fire;
    if (edit.ignite) {
        if (!live->tree || !same_fire(*live->tree, edit.ignite->expected)) return reject("Conflict: fire state changed. Revert to reload live values.");
        fire = *live->tree;
        if (fire.burning() || fire.charred || !contains(map_, position.hex) ||
            !simulation_detail::can_grow(map_.tiles[map_.index(position.hex)])) return reject("Only a healthy tree on grass or land can ignite.");
        const double seconds = edit.ignite->duration_seconds;
        if (!std::isfinite(seconds) || seconds < FireTuning::burn_seconds_min || seconds > FireTuning::burn_seconds_max ||
            std::floor(seconds * 4) != seconds * 4) return reject("Burn duration must be 12–20 seconds in 0.25 second steps.");
        fire.ignition_tick = tick_;
        fire.burnout_tick = tick_ + static_cast<std::uint64_t>(seconds * 32);
    }
    // No writes until the entire group validates. Supported setters notify Flecs.
    if (move) entity.set<ActorPosition>(position);
    if (change_appearance) entity.set<ActorAppearance>(appearance);
    if (change_selection) {
        if (selectable) entity.add<Selectable>(); else entity.remove<Selectable>();
    }
    if (edit.ignite) entity.set<Tree>(fire);
    if (move) rebuild_occupancy();
    return {pending.request, true, "Applied at the simulation command boundary."};
}
