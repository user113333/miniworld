#include "simulation/world.h"
#include "simulation/tuning.h"
#include "random.h"
#include "world/tree_placement.h"

#include <cmath>

using namespace simulation_detail;

Tree World::make_lifecycle(ActorId id) const
{
    Tree tree;
    tree.growth_ticks = duration(map_.seed, id, Sample::growth,
        ForestTuning::growth_seconds_min, ForestTuning::growth_seconds_max);
    tree.lifespan_ticks = duration(map_.seed, id, Sample::lifespan,
        ForestTuning::lifespan_seconds_min, ForestTuning::lifespan_seconds_max);
    tree.decay_ticks = duration(map_.seed, id, Sample::decay,
        ForestTuning::decay_seconds_min, ForestTuning::decay_seconds_max);
    return tree;
}

void World::grow_tree(int cell)
{
    TreeSpawn spawn{map_.hex_at(cell),
        static_cast<unsigned>(sample(map_.seed, tick_, cell, Sample::variant) % 3),
        {static_cast<float>(unit_sample(map_.seed, tick_, cell, Sample::offset_x)),
         static_cast<float>(unit_sample(map_.seed, tick_, cell, Sample::offset_y))}};
    spawn.offset = (spawn.offset - 0.5f) * 0.8f;
    Tree tree = make_lifecycle(next_actor_id_);
    tree.stage = TreeStage::growing;
    spawn_tree(spawn, tree);
}

void World::collect_births()
{
    births_.clear();
    for (int cell = 0; cell < static_cast<int>(map_.tiles.size()); ++cell) {
        const Terrain terrain = map_.tiles[cell];
        if (occupancy_[cell] != 0 || !can_grow(terrain)) continue;
        const double rate = terrain == Terrain::grass ? ForestTuning::grass_birth_rate
                                                      : ForestTuning::land_birth_rate;
        const double suitability = forest_suitability(map_.hex_at(cell), map_.seed);
        const double habitat = ForestTuning::habitat_birth_scale
                               * suitability * suitability * suitability;
        const double probability = -std::expm1(-rate * habitat * world_seconds);
        if (unit_sample(map_.seed, tick_, cell, Sample::birth) < probability) births_.push_back(cell);
    }
}

void World::advance_tree(flecs::entity entity, const ActorIdentity& id,
                          const ActorPosition& position, Tree& tree)
{
    if (tree.burning()) {
        if (tick_ >= tree.burnout_tick) {
            burn_down(id, position, tree);
        } else if (tree.stage != TreeStage::dead && tree.ignition_tick != tick_) {
            tree.age_ticks += world_ticks;
            if (tree.stage == TreeStage::growing && tree.age_ticks >= tree.growth_ticks)
                tree.stage = TreeStage::mature;
        }
        return;
    }
    if (tree.stage == TreeStage::dead) {
        tree.dead_ticks += world_ticks;
        if (tree.dead_ticks >= tree.decay_ticks) decayed_.push_back({entity.id(), generation_});
        return;
    }
    tree.age_ticks += world_ticks;
    if (tree.age_ticks >= tree.lifespan_ticks) {
        tree.stage = TreeStage::dead;
        tree.dead_ticks = 0;
    } else if (tree.stage == TreeStage::growing && tree.age_ticks >= tree.growth_ticks) {
        tree.stage = TreeStage::mature;
    }
}
