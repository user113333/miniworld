#include "simulation/world.h"
#include "simulation/tuning.h"
#include "random.h"
#include "world/hex_grid.h"

#include <algorithm>
#include <cmath>

using namespace simulation_detail;

namespace {
bool susceptible(const TileMap& map, const ActorView& actor)
{
    return actor.tree && !actor.tree->charred && !actor.tree->burning()
        && map.contains(actor.position.hex) && can_grow(map.tiles[map.index(actor.position.hex)]);
}
}

bool World::enqueue(IgniteActor command)
{
    const auto actor = inspect(command.target);
    if (!actor || !susceptible(map_, *actor) || pending_ignitions_.size() >= identities_.size()
        || std::any_of(pending_ignitions_.begin(), pending_ignitions_.end(), [&](IgniteActor pending) {
            return pending.target == command.target;
        })) return false;
    pending_ignitions_.push_back(command);
    return true;
}

void World::collect_ignitions()
{
    ignitions_.clear();
    bool burning = false;
    registry().each([&](const Tree& tree) { burning = burning || tree.burning(); });
    if (pending_ignitions_.empty() && !burning) return;
    std::vector<ActorId> commands;
    for (auto command : pending_ignitions_) {
        if (auto actor = inspect(command.target)) commands.push_back(actor->id);
    }
    std::sort(commands.begin(), commands.end());
    for (int cell = 0; cell < static_cast<int>(occupancy_.size()); ++cell) {
        const auto target = inspect(actor_at(cell));
        if (!target || !susceptible(map_, *target)) continue;
        if (std::binary_search(commands.begin(), commands.end(), target->id)) {
            ignitions_.push_back(cell);
            continue;
        }
        int neighbors = 0;
        for (glm::ivec2 direction : hex_neighbors) {
            const auto hex = target->position.hex + direction;
            if (!map_.contains(hex)) continue;
            const auto neighbor = inspect(actor_at(map_.index(hex)));
            neighbors += neighbor && neighbor->tree && neighbor->tree->burning();
        }
        if (neighbors == 0) continue;
        const double probability = -std::expm1(-FireTuning::spread_rate * neighbors * world_seconds);
        if (unit_sample(map_.seed, tick_, target->id, Sample::spread) < probability)
            ignitions_.push_back(cell);
    }
    pending_ignitions_.clear();
}

void World::apply_ignitions()
{
    for (int cell : ignitions_) {
        auto entity = registry().entity(occupancy_[cell]);
        const auto id = entity.get<ActorIdentity>().value;
        Tree& tree = entity.get_mut<Tree>();
        tree.ignition_tick = tick_;
        tree.burnout_tick = tick_ + duration(map_.seed, id, Sample::burn_duration,
            FireTuning::burn_seconds_min,
            FireTuning::burn_seconds_max);
    }
}

void World::burn_down(const ActorIdentity& id, const ActorPosition& position, Tree& tree)
{
    tree.charred = true;
    tree.burnout_tick = 0;
    tree.dead_ticks = 0;
    tree.decay_ticks = duration(map_.seed, id.value, Sample::charred_decay,
        FireTuning::charred_decay_seconds_min, FireTuning::charred_decay_seconds_max);
    const int cell = map_.index(position.hex);
    if (can_grow(map_.tiles[cell])) {
        map_.recovery[cell] = {map_.tiles[cell], tick_, duration(map_.seed, cell, Sample::recovery_duration,
            FireTuning::recovery_seconds_min, FireTuning::recovery_seconds_max)};
        map_.tiles[cell] = Terrain::burned;
    }
}
