#include "world_fixture.h"
#include "world/island_generator.h"
#include "test_support.h"

#include <algorithm>

namespace {
void expect_same_world(const test::MapState& a, const test::MapState& b)
{
    test::expect(a.tiles == b.tiles && a.trees.size() == b.trees.size(),
                 "Forest population or terrain changed");
    const auto other = test::trees_by_cell(b);
    for (const auto& tree : a.trees) {
        const auto& expected = other.at(a.index(tree.hex));
        test::expect_same_tree(tree, expected);
        test::expect(tree.id == expected.id && !tree.burning() && !tree.charred &&
                     tree.decay_ticks == 0 && tree.dead_ticks == 0,
                     "Healthy tree state changed");
    }
}

void check_persistent_forest()
{
    for (float density : {0.0f, 0.2f, 1.0f}) {
        test::MapState map = generate_island(24, 18, 74129);
        place_trees(map, density);
        test::WorldFixture simulation;
        simulation.reset(map);
        const auto original = map;
        auto reversed = map;
        std::reverse(reversed.trees.begin(), reversed.trees.end());
        test::WorldFixture reordered;
        reordered.reset(reversed);
        for (std::uint64_t tick = 8; tick <= 1200 * 32; tick += 8) {
            simulation.update(map, tick);
            reordered.update(reversed, tick);
        }
        expect_same_world(map, original);
        expect_same_world(reversed, original);
        for (const auto& tree : map.trees)
            test::expect(simulation.world.inspect(simulation.world.actor_at(map.index(tree.hex)))->id == tree.id,
                         "Persistent tree lost occupancy");
    }
}

void check_frame_replay()
{
    test::SimulationFixture coarse;
    test::SimulationFixture fine;
    for (test::SimulationFixture* island : {&coarse, &fine}) {
        island->settings.width = 24;
        island->settings.height = 18;
        island->settings.seed = 74129;
        island->settings.simulation_speed = 8.0f;
        island->regenerate();
    }
    const test::MapState initial = fine.map();
    for (int frame = 0; frame < 200; ++frame) {
        coarse.advance_simulation(0.25);
    }
    for (int frame = 0; frame < 3200; ++frame) {
        fine.advance_simulation(1.0 / 64.0);
    }
    test::expect(coarse.clock().ticks() == fine.clock().ticks(), "Frame partitions changed elapsed time");
    expect_same_world(coarse.map(), fine.map());
    const test::MapState paused = fine.map();
    fine.toggle_pause();
    for (int frame = 0; frame < 20; ++frame) {
        fine.advance_simulation(30.0);
    }
    expect_same_world(fine.map(), paused);
    fine.regenerate();
    expect_same_world(fine.map(), initial);
    test::expect(fine.clock().ticks() == 0 && fine.settings.simulation_speed == 0.0f,
                 "Reset retained live simulation time or changed pause state");
    fine.toggle_pause();
    fine.advance_simulation(0.25);
}

} // namespace

int main()
{
    return test::run("Persistent forest simulation", [] {
        check_persistent_forest();
        check_frame_replay();
    });
}
