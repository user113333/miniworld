#include "island_demo.h"
#include "test_support.h"

namespace {

void check_reset()
{
    IslandDemo island;
    island.settings.seed = 74129;
    island.settings.simulation_speed = 2.0f;
    island.regenerate();
    const TileMap original = island.map();
    const auto original_generation = island.world_generation();

    // One completed tick and half a tick waiting in the accumulator.
    island.advance_simulation(SimulationClock::base_step_seconds * 0.75);
    test::expect(island.clock().ticks() == 1, "Controller did not apply the chosen speed");
    island.regenerate();
    test::expect(island.world_generation() == original_generation + 1, "Regenerate kept the old generation");
    test::expect(island.settings.simulation_speed == 2.0f, "Regenerate changed speed");
    test::expect(island.clock().ticks() == 0, "Regenerate did not reset elapsed time");
    island.advance_simulation(SimulationClock::base_step_seconds * 0.25);
    test::expect(island.clock().ticks() == 0, "Regenerate retained fractional time");
    test::expect(island.map().tiles == original.tiles, "Regenerate changed seeded terrain");
    test::expect(island.map().trees.size() == original.trees.size(), "Regenerate changed tree count");
    for (std::size_t i = 0; i < original.trees.size(); ++i) {
        test::expect_same_tree(island.map().trees[i], original.trees[i]);
    }

    island.advance_simulation(0.25);
    island.generate();
    test::expect(island.world_generation() == original_generation + 2, "Generate kept the old generation");
    test::expect(island.clock().ticks() == 0, "Generate did not reset elapsed time");
    test::expect(island.settings.simulation_speed == 2.0f, "Generate changed speed");
    island.advance_simulation(SimulationClock::base_step_seconds * 0.25);
    test::expect(island.clock().ticks() == 0, "Generate retained fractional time");
}

void check_pause_and_inactivity()
{
    IslandDemo island;
    island.settings.simulation_speed = 3.0f;
    island.advance_simulation(SimulationClock::base_step_seconds / 6.0);
    island.toggle_pause();
    test::expect(island.settings.simulation_speed == 0.0f, "Pause did not select zero speed");
    island.advance_simulation(3600.0);
    island.regenerate();
    test::expect(island.settings.simulation_speed == 0.0f, "Regenerate unpaused the world");
    island.toggle_pause();
    test::expect(island.settings.simulation_speed == 3.0f, "Resume forgot the selected speed");
    island.advance_simulation(SimulationClock::base_step_seconds / 6.0);
    test::expect(island.clock().ticks() == 0, "Paused reset retained fractional time");
    island.advance_simulation(3600.0, false);
    test::expect(island.clock().ticks() == 0, "Inactive time advanced the simulation");
    island.advance_simulation(SimulationClock::base_step_seconds / 6.0);
    test::expect(island.clock().ticks() == 1, "Inactivity lost the existing fraction or caught up");
}

void check_live_world_and_initial_density()
{
    IslandDemo island;
    island.settings.seed = 74129;
    island.regenerate();
    const TileMap original = island.map();
    const auto generation = island.world_generation();
    island.settings.tree_density = 0.0f;
    test::expect(island.map().trees.size() == original.trees.size(),
                 "Editing initial density reset the current forest");
    island.settings.simulation_speed = 8.0f;
    island.advance_simulation(0.25);
    test::expect(island.clock().elapsed_seconds() == 2.0, "Simulation elapsed display is incorrect");
    const Tree& tree = island.map().trees.front();
    test::expect(tree.id == original.trees.front().id &&
                 tree.age_ticks == original.trees.front().age_ticks + 64,
                 "Controller did not drive living trees at world cadence");
    test::expect(island.world_generation() == generation, "Simulation replaced the whole forest");
    test::expect(island.map().tiles == original.tiles, "Natural forest advancement changed terrain");
    island.regenerate();
    test::expect(island.map().trees.empty() && island.clock().ticks() == 0,
                 "Regenerate did not apply the edited initial density");
}

} // namespace

int main()
{
    return test::run("Island simulation controller", [] {
        check_reset();
        check_pause_and_inactivity();
        check_live_world_and_initial_density();
    });
}
