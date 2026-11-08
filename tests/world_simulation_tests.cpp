#include "island_demo.h"
#include "hex_grid.h"
#include "island_generator.h"
#include "simulation_clock.h"
#include "test_support.h"
#include "tree_placement.h"
#include "world_simulation.h"

#include <algorithm>
#include <set>

namespace {

constexpr std::uint64_t world_ticks = SimulationClock::ticks_per_world_step;

TileMap flat_map(Terrain terrain, std::uint32_t seed, int width = 12, int height = 12)
{
    return {width, height, seed, std::vector<Terrain>(width * height, terrain), {}};
}

void expect_same_runtime(const Tree& actual, const Tree& expected)
{
    test::expect_same_tree(actual, expected);
    test::expect(actual.id == expected.id && actual.stage == expected.stage,
                 "Tree identity or stage changed during replay");
    test::expect(actual.age_ticks == expected.age_ticks && actual.dead_ticks == expected.dead_ticks,
                 "Tree age changed during replay");
    test::expect(actual.growth_ticks == expected.growth_ticks &&
                 actual.lifespan_ticks == expected.lifespan_ticks &&
                 actual.decay_ticks == expected.decay_ticks, "Lifecycle sampling changed during replay");
}

void expect_same_world(const TileMap& actual, const TileMap& expected)
{
    test::expect(actual.tiles == expected.tiles, "Terrain changed during replay");
    const auto actual_trees = test::trees_by_cell(actual);
    const auto expected_trees = test::trees_by_cell(expected);
    test::expect(actual_trees.size() == expected_trees.size(), "Replay changed population");
    for (const auto& [cell, tree] : expected_trees) {
        const auto found = actual_trees.find(cell);
        test::expect(found != actual_trees.end(), "Replay changed occupied cells");
        expect_same_runtime(found->second, tree);
    }
}

void check_invariants(const TileMap& map, const WorldSimulation& simulation)
{
    const auto occupied = test::trees_by_cell(map);
    std::set<std::uint64_t> ids;
    test::expect(simulation.tree_index_at(-1) == -1 &&
                 simulation.tree_index_at(static_cast<int>(map.tiles.size())) == -1,
                 "Outside occupancy query returned a tree");
    for (int cell = 0; cell < static_cast<int>(map.tiles.size()); ++cell) {
        const int index = simulation.tree_index_at(cell);
        if (!occupied.count(cell)) {
            test::expect(index == -1, "Removed tree left stale occupancy");
            continue;
        }
        test::expect(index >= 0 && static_cast<std::size_t>(index) < map.trees.size(),
                     "Occupied cell has an invalid vector index");
        const Tree& tree = map.trees[index];
        test::expect(map.index(tree.hex) == cell, "Occupancy refers to a different cell");
        test::expect(map.tiles[cell] == Terrain::grass || map.tiles[cell] == Terrain::land,
                     "Birth occurred on ineligible terrain");
        test::expect(tree.id != 0 && ids.insert(tree.id).second, "Tree ID is zero or duplicated");
        test::expect(tree.variant < 3 && std::isfinite(tree.offset.x) && std::isfinite(tree.offset.y),
                     "Newborn has invalid sprite metadata");
        test::expect(glm::length(tree.offset) < glm::sqrt(3.0f) * 0.5f,
                     "Newborn ground anchor is outside its tile");
        for (std::uint64_t duration : {tree.growth_ticks, tree.lifespan_ticks, tree.decay_ticks}) {
            test::expect(duration > 0 && duration % world_ticks == 0,
                         "Lifecycle duration is not on a world boundary");
        }
        if (tree.stage == TreeStage::dead) {
            test::expect(tree.dead_ticks < tree.decay_ticks, "Expired corpse retained occupancy");
        }
    }
}

void check_initial_state()
{
    TileMap map = generate_island(48, 32, 74129);
    place_trees(map, 0.2f);
    const TileMap original = map;
    WorldSimulation simulation;
    simulation.reset(map);
    std::set<std::uint64_t> ages;
    std::uint64_t previous_id = 0;
    for (const auto& [cell, tree] : test::trees_by_cell(map)) {
        test::expect(tree.id > previous_id, "Initial IDs do not follow cell order");
        previous_id = tree.id;
        test::expect(tree.stage == TreeStage::mature, "Initial forest is not mature");
        test::expect(tree.age_ticks >= tree.growth_ticks &&
                     tree.age_ticks <= tree.lifespan_ticks * 3 / 5,
                     "Initial age is not safely below natural death");
        test::expect(tree.growth_ticks >= 15 * 32 && tree.growth_ticks <= 30 * 32,
                     "Growth duration is outside planned tuning");
        test::expect(tree.lifespan_ticks >= 180 * 32 && tree.lifespan_ticks <= 360 * 32,
                     "Lifespan is outside planned tuning");
        test::expect(tree.decay_ticks >= 30 * 32 && tree.decay_ticks <= 60 * 32,
                     "Decay duration is outside planned tuning");
        ages.insert(tree.age_ticks);
    }
    test::expect(ages.size() > 10, "Initial forest ages are synchronized");
    test::expect(map.tiles == original.tiles && map.trees.size() == original.trees.size(),
                 "Runtime initialization changed the initial forest");
    for (std::size_t i = 0; i < map.trees.size(); ++i) {
        test::expect_same_tree(map.trees[i], original.trees[i]);
    }
    check_invariants(map, simulation);
}

void expect_clustered_forest(const TileMap& map, const WorldSimulation& simulation)
{
    int living = 0;
    int grouped = 0;
    for (const Tree& tree : map.trees) {
        if (tree.stage == TreeStage::dead) {
            continue;
        }
        ++living;
        int neighbors = 0;
        for (glm::ivec2 direction : hex_neighbors) {
            const glm::ivec2 hex = tree.hex + direction;
            if (!map.contains(hex)) {
                continue;
            }
            const int index = simulation.tree_index_at(map.index(hex));
            neighbors += index >= 0 && map.trees[index].stage != TreeStage::dead;
        }
        grouped += neighbors >= 2;
    }
    test::expect(living > 0 && grouped * 2 > living,
                 "Forest turnover lost coherent living patches");
}

void check_lifecycle_boundaries()
{
    TileMap map = flat_map(Terrain::water, 123);
    map.tiles[map.index({0, 0})] = Terrain::grass;
    map.trees.push_back({{0, 0}, 2, {0.1f, -0.1f}});
    WorldSimulation simulation;
    simulation.reset(map);
    Tree& tree = map.trees.front();
    const auto id = tree.id;
    tree.stage = TreeStage::growing;
    tree.age_ticks = tree.growth_ticks - world_ticks;
    simulation.update(map, 8);
    test::expect(map.trees.front().stage == TreeStage::mature, "Sapling missed its growth boundary");
    test::expect(map.trees.front().id == id && map.trees.front().variant == 2,
                 "Growth changed stable identity or mature sprite");
    map.trees.front().age_ticks = map.trees.front().lifespan_ticks - world_ticks;
    simulation.update(map, 16);
    test::expect(map.trees.front().stage == TreeStage::dead && map.trees.front().dead_ticks == 0,
                 "Natural death missed its boundary or consumed decay time immediately");
    simulation.update(map, 24);
    test::expect(map.trees.front().dead_ticks == world_ticks, "Dead wood did not age gradually");
    map.trees.front().dead_ticks = map.trees.front().decay_ticks - world_ticks;
    simulation.update(map, 32);
    test::expect(map.trees.empty() && simulation.tree_index_at(map.index({0, 0})) == -1,
                 "Decay did not remove both corpse and occupancy");
    test::expect(map.tiles[map.index({0, 0})] == Terrain::grass, "Natural death changed terrain");
}

void check_birth_snapshot()
{
    // Find a successful deterministic birth, then put an expiring corpse on its
    // cell. The same trial must be blocked by start-of-step occupancy.
    for (std::uint32_t seed = 0; seed < 1024; ++seed) {
        TileMap empty = flat_map(Terrain::grass, seed);
        WorldSimulation births;
        births.reset(empty);
        births.update(empty, 8);
        if (empty.trees.empty()) {
            continue;
        }
        const Tree newborn = empty.trees.front();
        test::expect(newborn.stage == TreeStage::growing && newborn.age_ticks == 0,
                     "Newborn was updated again on its birth step");
        TileMap occupied = flat_map(Terrain::grass, seed);
        occupied.trees.push_back(newborn);
        WorldSimulation simulation;
        simulation.reset(occupied);
        occupied.trees.front().stage = TreeStage::dead;
        occupied.trees.front().dead_ticks = occupied.trees.front().decay_ticks - world_ticks;
        simulation.update(occupied, 8);
        test::expect(simulation.tree_index_at(occupied.index(newborn.hex)) == -1,
                     "A corpse was replaced during the step that removed it");
        check_invariants(occupied, simulation);
        return;
    }
    test::expect(false, "Reference seeds did not exercise a birth trial");
}

void check_empty_and_full_maps()
{
    for (Terrain terrain : {Terrain::water, Terrain::sand}) {
        TileMap map = flat_map(terrain, 1);
        WorldSimulation simulation;
        simulation.reset(map);
        for (std::uint64_t tick = 8; tick <= 3200; tick += 8) {
            simulation.update(map, tick);
        }
        test::expect(map.trees.empty(), "Water or sand supported tree births");
        check_invariants(map, simulation);
    }
    TileMap full = flat_map(Terrain::grass, 2);
    place_trees(full, 1.0f);
    WorldSimulation simulation;
    simulation.reset(full);
    simulation.update(full, 8);
    test::expect(full.trees.size() == full.tiles.size(), "Full occupancy lost or duplicated a tree");
    check_invariants(full, simulation);
}

void check_birth_weighting()
{
    std::size_t grass_count = 0;
    std::size_t land_count = 0;
    double habitat_suitability = 0;
    double birth_suitability = 0;
    std::size_t habitat_count = 0;
    for (std::uint32_t seed = 0; seed < 24; ++seed) {
        TileMap grass = flat_map(Terrain::grass, seed, 48, 32);
        TileMap land = flat_map(Terrain::land, seed, 48, 32);
        WorldSimulation grass_simulation;
        WorldSimulation land_simulation;
        grass_simulation.reset(grass);
        land_simulation.reset(land);
        for (std::uint64_t tick = 8; tick <= 640; tick += 8) {
            grass_simulation.update(grass, tick);
            land_simulation.update(land, tick);
        }
        grass_count += grass.trees.size();
        land_count += land.trees.size();
        for (int cell = 0; cell < static_cast<int>(grass.tiles.size()); ++cell) {
            habitat_suitability += forest_suitability(grass.hex_at(cell), seed);
            ++habitat_count;
        }
        for (const Tree& tree : grass.trees) {
            birth_suitability += forest_suitability(tree.hex, seed);
        }
        check_invariants(grass, grass_simulation);
        check_invariants(land, land_simulation);
    }
    test::expect(land_count > 0 && grass_count > 2 * land_count,
                 "Equal-habitat fixed-seed trials do not favor grass births");
    test::expect(birth_suitability / grass_count > habitat_suitability / habitat_count + 0.05,
                 "Births do not favor coherent forest patches over uniform scattering");
}

void check_reordering_and_long_run()
{
    TileMap map = generate_island(48, 32, 74129);
    place_trees(map, 0.2f);
    TileMap reversed = map;
    std::reverse(reversed.trees.begin(), reversed.trees.end());
    WorldSimulation simulation;
    WorldSimulation reordered_simulation;
    simulation.reset(map);
    reordered_simulation.reset(reversed);
    expect_same_world(map, reversed);
    std::set<std::uint64_t> original_ids;
    for (const Tree& tree : map.trees) {
        original_ids.insert(tree.id);
    }
    const auto original_terrain = map.tiles;
    bool saw_growing = false;
    bool saw_dead = false;
    for (std::uint64_t tick = 8; tick <= 1200 * 32; tick += 8) {
        simulation.update(map, tick);
        reordered_simulation.update(reversed, tick);
        for (const Tree& tree : map.trees) {
            saw_growing |= tree.stage == TreeStage::growing;
            saw_dead |= tree.stage == TreeStage::dead;
        }
        if (tick == 600 * 32) {
            expect_clustered_forest(map, simulation);
        }
        if (tick % (16 * 32) == 0) {
            check_invariants(map, simulation);
            expect_same_world(map, reversed);
            test::expect(map.trees.size() <= map.tiles.size(), "Long run exceeded bounded occupancy");
        }
    }
    test::expect(saw_growing && saw_dead && !map.trees.empty(),
                 "Long run did not sustain gradual births and deaths");
    test::expect(map.tiles == original_terrain, "Natural forest turnover changed terrain");
    for (const Tree& tree : map.trees) {
        test::expect(!original_ids.count(tree.id) && tree.id > *original_ids.rbegin(),
                     "An original tree survived indefinitely or its ID was reused");
    }
}

void check_frame_replay()
{
    IslandDemo coarse;
    IslandDemo fine;
    for (IslandDemo* island : {&coarse, &fine}) {
        island->settings.width = 24;
        island->settings.height = 18;
        island->settings.seed = 74129;
        island->settings.simulation_speed = 8.0f;
        island->regenerate();
    }
    const TileMap initial = fine.map();
    for (int frame = 0; frame < 200; ++frame) {
        coarse.advance_simulation(0.25);
    }
    for (int frame = 0; frame < 3200; ++frame) {
        fine.advance_simulation(1.0 / 64.0);
    }
    test::expect(coarse.clock().ticks() == fine.clock().ticks(), "Frame partitions changed elapsed time");
    expect_same_world(coarse.map(), fine.map());
    const TileMap paused = fine.map();
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
    return test::run("Living forest simulation", [] {
        check_initial_state();
        check_lifecycle_boundaries();
        check_birth_snapshot();
        check_empty_and_full_maps();
        check_birth_weighting();
        check_reordering_and_long_run();
        check_frame_replay();
    });
}
