#include "fire_effects.h"
#include "island_demo.h"
#include "test_support.h"

namespace {

void expect_same_world(const TileMap& actual, const TileMap& expected)
{
    test::expect(actual.tiles == expected.tiles, "Effects changed terrain replay");
    test::expect(actual.recovery.size() == expected.recovery.size(), "Reset left stale recovery data");
    for (std::size_t i = 0; i < actual.recovery.size(); ++i) {
        const auto& a = actual.recovery[i];
        const auto& b = expected.recovery[i];
        test::expect(a.original_terrain == b.original_terrain && a.started_tick == b.started_tick &&
                     a.duration_ticks == b.duration_ticks, "Effects changed burned ground");
    }
    const auto actual_trees = test::trees_by_cell(actual);
    const auto expected_trees = test::trees_by_cell(expected);
    test::expect(actual_trees.size() == expected_trees.size(), "Effects changed tree count");
    for (const auto& [cell, b] : expected_trees) {
        const auto found = actual_trees.find(cell);
        test::expect(found != actual_trees.end(), "Effects changed tree occupancy");
        const Tree& a = found->second;
        test::expect_same_tree(a, b);
        test::expect(a.id == b.id && a.stage == b.stage && a.age_ticks == b.age_ticks &&
                     a.growth_ticks == b.growth_ticks && a.lifespan_ticks == b.lifespan_ticks &&
                     a.decay_ticks == b.decay_ticks && a.dead_ticks == b.dead_ticks &&
                     a.ignition_tick == b.ignition_tick && a.burnout_tick == b.burnout_tick &&
                     a.charred == b.charred, "Effects or frame partition changed live state");
    }
}

TileMap replay(std::size_t capacity, int frames, double frame_seconds)
{
    IslandDemo island;
    island.settings.width = 24;
    island.settings.height = 18;
    island.settings.seed = 74129;
    island.settings.tree_density = 1.0f;
    island.settings.simulation_speed = 50.0f;
    island.regenerate();
    ParticleSystem effects(capacity);
    effects.reset(island.map().seed);
    std::vector<FireEmitter> emitters;
    const auto generation = island.world_generation();
    const auto first_id = island.map().trees.front().id;
    const auto second_id = island.map().trees.back().id;
    test::expect(island.queue_ignition(first_id, generation), "Could not queue replay fire");
    for (int frame = 0; frame < frames; ++frame) {
        island.accumulate_simulation_time(frame_seconds);
        while (island.step_simulation()) {
            if (island.clock().ticks() == 128) {
                island.queue_ignition(second_id, generation);
            }
            if (island.clock().world_update_due()) {
                collect_fire_emitters(island.map(), emitters);
                effects.set_emitters(emitters);
            }
            effects.step(island.clock().ticks());
        }
    }
    test::expect(island.clock().elapsed_seconds() == 100.0, "Replay consumed the wrong shared time");
    bool burned = false;
    for (Terrain terrain : island.map().tiles) {
        burned |= terrain == Terrain::burned;
    }
    test::expect(burned, "Replay never exercised fire death");
    if (capacity > 0) {
        test::expect(effects.spawned_count() > 0, "Real fire did not produce visual particles");
    }
    return island.map();
}

void check_paused_commands_and_reset()
{
    IslandDemo island;
    island.settings.seed = 74129;
    island.regenerate();
    const TileMap original = island.map();
    const auto generation = island.world_generation();
    const auto id = island.map().trees.front().id;
    island.settings.simulation_speed = 0.0f;
    test::expect(island.queue_ignition(id, generation), "Paused ignition was not queued");
    island.advance_simulation(100.0);
    test::expect(island.pending_ignitions() == 1, "Pause discarded the queued command");
    expect_same_world(island.map(), original);
    island.settings.simulation_speed = 1.0f;
    island.advance_simulation(0.25);
    test::expect(island.pending_ignitions() == 0 && island.map().trees.front().burning(),
                 "Queued fire did not start on the first resumed world step");
    island.settings.ignite_trees = true;
    island.regenerate();
    test::expect(!island.settings.ignite_trees && island.pending_ignitions() == 0,
                 "Regeneration retained tool selection or pending fire commands");
    test::expect(!island.queue_ignition(id, generation), "Old-world click targeted a reused tree ID");
    expect_same_world(island.map(), original);
}

} // namespace

int main()
{
    return test::run("Shared fire and visual integration", [] {
        const TileMap disabled = replay(0, 8, 0.25);
        expect_same_world(replay(1, 128, 1.0 / 64.0), disabled);
        expect_same_world(replay(ParticleSystem::max_particles, 8, 0.25), disabled);
        check_paused_commands_and_reset();
    });
}
