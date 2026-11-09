#include "simulation/world.h"
#include "simulation/tuning.h"
#include "world/navigation.h"
#include "random.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr double dt = SimulationClock::base_step_seconds;
constexpr std::uint64_t ticks(double seconds) { return static_cast<std::uint64_t>(seconds * 32); }
float random_unit(Settler& s)
{
    s.random_state = simulation_detail::mix(s.random_state + 0x9e3779b97f4a7c15ULL);
    return static_cast<float>((s.random_state >> 40) / 16777216.0);
}
void say(Settler& s, const char* text)
{
    s.thought = text;
    s.thought_ticks = ticks(SettlerTuning::thought_seconds);
}
bool plantable(const World& world, glm::ivec2 hex)
{
    if (!world.map().contains(hex)) return false;
    const auto terrain = world.map().tiles[world.map().index(hex)];
    return (terrain == Terrain::grass || terrain == Terrain::land)
        && !world.actor_at(world.map().index(hex));
}
void finish(Settler& s, bool failed = false)
{
    if (s.action != SettlerAction::idle && s.action_ticks)
        s.mean_active_seconds = 0.8 * s.mean_active_seconds + 0.2 * s.action_ticks * dt;
    s.action = SettlerAction::idle;
    s.remaining_ticks = failed ? ticks(1) : 0;
    s.action_ticks = 0;
    s.route.clear();
    s.target = {};
}
}

ActorHandle World::spawn_settler(glm::ivec2 hex, glm::vec2 offset)
{
    if (!walkable(map_, hex) || !std::isfinite(offset.x) || !std::isfinite(offset.y) ||
        std::abs(offset.x) > glm::sqrt(3.0f) / 2 ||
        std::abs(offset.y) > 1 - std::abs(offset.x) / glm::sqrt(3.0f)) return {};
    const ActorId id = next_actor_id_++;
    Settler s;
    s.random_state = simulation_detail::mix(map_.seed ^ simulation_detail::mix(id));
    s.dna.speed = 0.5f + random_unit(s);
    for (auto& weight : s.dna.preferences) weight = 0.75f + random_unit(s) * 0.5f;
    s.dna.people_sensitivity = 0.5f + random_unit(s);
    s.dna.tree_sensitivity = 0.5f + random_unit(s);
    s.next_thought = settler_tick_ + ticks(10 + random_unit(s) * 20);
    const auto entity = create_settler(registry(), id, {hex, offset}, s);
    identities_[id] = entity.id();
    return {entity.id(), generation_};
}

void World::populate_settlers()
{
    // Reservoir-style partial shuffle; separate starting cells, independent of forest generation.
    std::vector<glm::ivec2> candidates;
    for (int cell = 0; cell < static_cast<int>(map_.tiles.size()); ++cell)
        if (walkable(map_, map_.hex_at(cell))) candidates.push_back(map_.hex_at(cell));
    auto state = simulation_detail::mix(map_.seed);
    for (int i = 0; i < 3 && !candidates.empty(); ++i) {
        state = simulation_detail::mix(state + 0x9e3779b97f4a7c15ULL);
        const auto index = state % candidates.size();
        spawn_settler(candidates[index]);
        candidates[index] = candidates.back();
        candidates.pop_back();
    }
}

void World::choose_action(const ActorPosition& position, Settler& s)
{
    std::array<std::vector<glm::ivec2>, 4> candidates;
    // Only scan the bounded axial disk; occupancy lookup is constant time.
    for (int q = -SettlerTuning::search_radius; q <= SettlerTuning::search_radius; ++q) {
        for (int r = -SettlerTuning::search_radius; r <= SettlerTuning::search_radius; ++r) {
            const auto hex = position.hex + glm::ivec2{q, r};
            if (hex_distance(hex, position.hex) > SettlerTuning::search_radius || !walkable(map_, hex)) continue;
            if (hex != position.hex) candidates[1].push_back(hex);
            const auto actor = inspect(actor_at(map_.index(hex)));
            if (actor && actor->tree && actor->tree->fully_grown()) candidates[2].push_back(hex);
            if (s.inventory.count(ItemType::seeds) && plantable(*this, hex)) candidates[3].push_back(hex);
        }
    }
    float active_weight = 0;
    for (int i = 1; i < 4; ++i) if (!candidates[i].empty()) active_weight += s.dna.preferences[i];
    std::array<float, 4> weights{4 * s.dna.preferences[0], 0, 0, 0};
    for (int i = 1; i < 4; ++i)
        if (!candidates[i].empty()) weights[i] = s.dna.preferences[i] / active_weight;
    float choice = random_unit(s) * (weights[0] + (active_weight > 0 ? 1 : 0));
    int action = 0;
    while (action < 3 && choice >= weights[action]) choice -= weights[action++];
    s.action = static_cast<SettlerAction>(action);
    s.action_ticks = 0;
    if (action == 0) {
        // Equal mean idle/active durations make the 4:1 decision weight approximately
        // 80% by elapsed time, even when route lengths change the active duration.
        s.remaining_ticks = ticks(std::clamp(s.mean_active_seconds * (0.75 + random_unit(s) * 0.5), 1.0, 120.0));
        say(s, "I'll rest");
        return;
    }
    auto& options = candidates[action];
    s.destination = options[static_cast<std::size_t>(random_unit(s) * options.size())];
    s.route = find_route(map_, position.hex, s.destination);
    s.waypoint = 0; // Return to the current cell center before following center-to-center edges.
    if (s.route.empty()) { finish(s, true); return; }
    s.target = action == 2 ? actor_at(map_.index(s.destination)) : ActorHandle{};
    s.remaining_ticks = ticks(action == 2 ? SettlerTuning::cut_seconds : SettlerTuning::plant_seconds);
    say(s, action == 1 ? "I'll explore" : action == 2 ? "I'll cut that tree" : "Time to plant");
}

void World::advance_settler(ActorPosition& position, Settler& s)
{
    if (s.thought_ticks && --s.thought_ticks == 0) s.thought.clear();
    if (settler_tick_ >= s.next_thought) {
        int people = 0, trees = 0;
        registry().each([&](const ActorPosition& other, const Settler&) {
            if (hex_distance(position.hex, other.hex) <= SettlerTuning::thought_radius) ++people;
        });
        for (int q = -SettlerTuning::thought_radius; q <= SettlerTuning::thought_radius; ++q)
            for (int r = -SettlerTuning::thought_radius; r <= SettlerTuning::thought_radius; ++r) {
            const auto hex = position.hex + glm::ivec2{q, r};
            if (hex_distance(position.hex, hex) > SettlerTuning::thought_radius || !map_.contains(hex)) continue;
            const auto actor = inspect(actor_at(map_.index(hex)));
            if (actor && actor->tree && !actor->tree->charred) ++trees;
        }
        const int people_cost = static_cast<int>(std::max(0, people - 1 - SettlerTuning::comfortable_people)
            * SettlerTuning::people_penalty * s.dna.people_sensitivity);
        const int tree_cost = static_cast<int>(std::max(0, trees - SettlerTuning::comfortable_trees)
            * SettlerTuning::tree_penalty * s.dna.tree_sensitivity);
        s.happiness = std::clamp(s.happiness + (people_cost || tree_cost ? -people_cost-tree_cost : SettlerTuning::comfort_gain), 0, 1000);
        say(s, people_cost ? "Too many people here" : tree_cost ? "Too many trees here" : "I like it here");
        s.next_thought = settler_tick_ + ticks(10 + random_unit(s) * 20);
    }
    if (!s.happiness) return;
    if (s.action == SettlerAction::idle) {
        ++s.idle_ticks;
        if (s.remaining_ticks) --s.remaining_ticks;
        else choose_action(position, s);
        return;
    }
    ++s.active_ticks;
    ++s.action_ticks;
    if (s.action == SettlerAction::cut) {
        const auto target = inspect(s.target);
        if (!target || !target->tree || !target->tree->fully_grown() || target->position.hex != s.destination) {
            finish(s, true); return;
        }
    } else if (s.action == SettlerAction::plant && !plantable(*this, s.destination)) {
        finish(s, true); return;
    }
    if (s.waypoint < s.route.size()) {
        const auto next = s.route[s.waypoint];
        if (!walkable(map_, position.hex) || !walkable(map_, next) || hex_distance(position.hex, next) > 1) {
            finish(s, true); return;
        }
        const auto ground = hex_center(position.hex) + position.offset;
        const auto delta = hex_center(next) - ground;
        const float distance = glm::length(delta);
        const float step = SettlerTuning::base_speed * s.dna.speed * static_cast<float>(dt);
        const auto moved = distance <= step ? hex_center(next) : ground + delta * (step / distance);
        position.hex = world_hex(moved);
        position.offset = moved - hex_center(position.hex);
        if (distance <= step) ++s.waypoint;
        return;
    }
    if (s.action == SettlerAction::wander) { finish(s); return; }
    if (s.remaining_ticks && --s.remaining_ticks != 0) return;
    if (s.action == SettlerAction::cut) {
        if (destroy(s.target)) { s.inventory.add(ItemType::seeds, 2); s.inventory.add(ItemType::logs, 4); }
    } else if (s.inventory.consume(ItemType::seeds)) {
        Tree tree;
        tree.growth_ticks = ticks(SettlerTuning::growth_seconds);
        spawn_tree({s.destination, static_cast<unsigned>(random_unit(s) * 3), {}}, tree);
    }
    finish(s);
}

void World::advance_settlers()
{
    // Collect handles first: completion may change archetypes or destroy actors.
    std::vector<ActorHandle> settlers;
    registry().each([&](flecs::entity e, const Settler&) { settlers.push_back({e.id(), generation_}); });
    std::sort(settlers.begin(), settlers.end(), [&](ActorHandle a, ActorHandle b) {
        return registry().entity(a.entity).get<ActorIdentity>().value < registry().entity(b.entity).get<ActorIdentity>().value;
    });
    for (const auto handle : settlers) {
        auto entity = registry().entity(handle.entity);
        // Copy state because planting can move Flecs tables while this settler is being updated.
        auto s = entity.get<Settler>();
        auto position = entity.get<ActorPosition>();
        if (s.happiness > 0) advance_settler(position, s);
        if (s.happiness <= 0) destroy(handle);
        else { entity.set<ActorPosition>(position); entity.set<Settler>(s); }
    }
}
