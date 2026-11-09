#include "world_fixture.h"
#include "simulation/simulation.h"
#include "test_support.h"
#include "world/tree_placement.h"
#include "simulation/world.h"

#include <algorithm>

namespace {

test::MapState flat_map(Terrain terrain, std::uint32_t seed)
{
    return {12, 12, seed, std::vector<Terrain>(144, terrain), {}};
}

void advance(test::WorldFixture& simulation, test::MapState& map, std::uint64_t& tick,
             std::uint64_t until)
{
    while (tick < until) {
        tick += 8;
        simulation.update(map, tick);
    }
}

void expect_cleared(const BurnRecovery& recovery)
{
    test::expect(recovery.original_terrain == Terrain::water && recovery.started_tick == 0 &&
                 recovery.duration_ticks == 0 && recovery.elapsed_ticks == 0 && !recovery.weathered(),
                 "Completed/reset recovery retained progress or original terrain");
}

void check_recovery_without_regrowth(Terrain original)
{
    test::MapState map = flat_map(Terrain::water, 74129);
    const int cell = map.index({0, 0});
    map.tiles[cell] = original;
    map.trees.push_back({{0, 0}, 0, {}});
    test::WorldFixture simulation;
    simulation.reset(map);
    const auto original_id = map.trees.front().id;
    test::expect(simulation.queue_ignition(map, original_id), "Could not start recovery fixture fire");
    std::uint64_t tick = 0;
    advance(simulation, map, tick, 8);
    const auto burnout = map.trees.front().burnout_tick;
    advance(simulation, map, tick, burnout);
    const BurnRecovery initial = map.recovery[cell];
    test::expect(map.tiles[cell] == Terrain::burned && initial.original_terrain == original &&
                 initial.started_tick == tick && initial.elapsed_ticks == 0 && !initial.weathered(),
                 "New burn did not start at the charred recovery stage");
    test::expect(initial.duration_ticks >= 90 * 32 && initial.duration_ticks <= 180 * 32 &&
                 initial.duration_ticks % 8 == 0, "Recovery duration is outside bounded seeded tuning");

    const auto before_half = ((initial.duration_ticks / 2 - 1) / 8) * 8;
    advance(simulation, map, tick, burnout + before_half);
    test::expect(map.tiles[cell] == Terrain::burned && !map.recovery[cell].weathered() &&
                 map.recovery[cell].elapsed_ticks == before_half,
                 "Charred ground weathered before its midpoint");
    test::expect(map.trees.empty() && simulation.tree_index_at(cell) == -1,
                 "Recovery did not continue independently after charred remains decayed");
    advance(simulation, map, tick, tick + 8);
    test::expect(map.tiles[cell] == Terrain::burned && map.recovery[cell].weathered(),
                 "Ground missed its weathering boundary or restored too early");
    advance(simulation, map, tick, burnout + initial.duration_ticks - 8);
    test::expect(map.tiles[cell] == Terrain::burned && map.trees.empty() &&
                 map.recovery[cell].elapsed_ticks < map.recovery[cell].duration_ticks,
                 "A tree respawned on unfinished recovery");
    advance(simulation, map, tick, tick + 8);
    test::expect(map.tiles[cell] == original && map.trees.empty(),
                 "Recovery changed the original type or spawned a tree on its completion step");
    expect_cleared(map.recovery[cell]);

    advance(simulation, map, tick, tick + 1200 * 32);
    test::expect(map.trees.empty() && map.tiles[cell] == original,
                 "Recovered terrain created trees");
}

void expect_same_world(const test::MapState& actual, const test::MapState& expected)
{
    test::expect(actual.tiles == expected.tiles && actual.recovery.size() == expected.recovery.size(),
                 "Replay changed terrain or recovery dimensions");
    for (std::size_t cell = 0; cell < actual.recovery.size(); ++cell) {
        const BurnRecovery& a = actual.recovery[cell];
        const BurnRecovery& b = expected.recovery[cell];
        test::expect(a.original_terrain == b.original_terrain && a.started_tick == b.started_tick &&
                     a.duration_ticks == b.duration_ticks && a.elapsed_ticks == b.elapsed_ticks,
                     "Frame partition or pause changed recovery state");
    }
    const auto trees = test::trees_by_cell(actual);
    const auto other = test::trees_by_cell(expected);
    test::expect(trees.size() == other.size(), "Replay changed living population");
    for (const auto& [cell, a] : trees) {
        const auto found = other.find(cell);
        test::expect(found != other.end(), "Replay changed occupancy");
        const test::TreeRecord& b = found->second;
        test::expect_same_tree(a, b);
        test::expect(a.id == b.id && a.decay_ticks == b.decay_ticks && a.dead_ticks == b.dead_ticks &&
                     a.ignition_tick == b.ignition_tick && a.burnout_tick == b.burnout_tick &&
                     a.charred == b.charred, "Replay changed tree lifecycle or fire state");
    }
}

void check_clock_replay_pause_and_reset()
{
    test::SimulationFixture coarse;
    test::SimulationFixture fine;
    for (test::SimulationFixture* island : {&coarse, &fine}) {
        island->settings.width = 24;
        island->settings.height = 18;
        island->settings.seed = 74129;
        island->settings.tree_density = 1.0f;
        island->settings.simulation_speed = 50.0f;
        island->regenerate();
    }
    const test::MapState original = coarse.map();
    for (test::SimulationFixture* island : {&coarse, &fine}) {
        for (const test::TreeRecord& tree : island->map().trees) {
            test::expect(island->queue_ignition(tree.id, island->world_generation()),
                         "Could not queue replay forest ignition");
        }
    }
    for (int frame = 0; frame < 12; ++frame) {
        coarse.advance_simulation(0.25);
    }
    for (int frame = 0; frame < 192; ++frame) {
        fine.advance_simulation(1.0 / 64.0);
    }
    expect_same_world(coarse.map(), fine.map());
    test::expect(std::any_of(coarse.map().recovery.begin(), coarse.map().recovery.end(),
                 [](const BurnRecovery& state) { return state.weathered(); }),
                 "Replay did not exercise weathered ground");
    const test::MapState paused = coarse.map();
    coarse.settings.simulation_speed = 0.0f;
    coarse.advance_simulation(100.0);
    expect_same_world(coarse.map(), paused);
    coarse.settings.simulation_speed = 50.0f;
    for (int frame = 0; frame < 20; ++frame) {
        coarse.advance_simulation(0.25);
    }
    for (int frame = 0; frame < 320; ++frame) {
        fine.advance_simulation(1.0 / 64.0);
    }
    expect_same_world(coarse.map(), fine.map());
    test::expect(coarse.clock().elapsed_seconds() == 400 && coarse.map().tiles == original.tiles &&
                 coarse.map().trees.empty(), "Complete recovery did not restore terrain without regrowth");
    for (const BurnRecovery& state : coarse.map().recovery) {
        expect_cleared(state);
    }
    coarse.settings.ignite_trees = true;
    coarse.regenerate();
    expect_same_world(coarse.map(), original);
    test::expect(coarse.clock().ticks() == 0 && coarse.pending_ignitions() == 0 &&
                 !coarse.settings.ignite_trees, "Reset retained elapsed time, pending fire or tool state");
}

void check_reset_active_scars()
{
    test::SimulationFixture island;
    island.settings.width = 24;
    island.settings.height = 18;
    island.settings.seed = 74129;
    island.settings.tree_density = 1.0f;
    island.settings.simulation_speed = 50.0f;
    island.regenerate();
    const test::MapState original = island.map();
    island.queue_ignition(island.map().trees.front().id, island.world_generation());
    island.advance_simulation(0.25);
    island.advance_simulation(0.25);
    test::expect(std::any_of(island.map().recovery.begin(), island.map().recovery.end(),
                 [](const BurnRecovery& state) { return state.duration_ticks != 0; }),
                 "Reset fixture did not leave an active scar");
    const auto fuel = std::find_if(island.map().trees.begin(), island.map().trees.end(),
        [](const test::TreeRecord& tree) { return !tree.charred && !tree.burning(); });
    test::expect(fuel != island.map().trees.end() &&
                 island.queue_ignition(fuel->id, island.world_generation()),
                 "Reset fixture could not retain a pending ignition");
    island.settings.ignite_trees = true;
    island.regenerate();
    expect_same_world(island.map(), original);
    test::expect(island.pending_ignitions() == 0 && island.clock().ticks() == 0 &&
                 !island.settings.ignite_trees, "Regenerate retained an active scar or pending state");
}

} // namespace

int main()
{
    return test::run("Burned ground recovery", [] {
        check_recovery_without_regrowth(Terrain::grass);
        check_recovery_without_regrowth(Terrain::land);
        check_clock_replay_pause_and_reset();
        check_reset_active_scars();
    });
}
