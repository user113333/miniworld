#include "hex_grid.h"
#include "test_support.h"
#include "world_simulation.h"

#include <algorithm>
#include <limits>

namespace {

TileMap fuel_map(std::uint32_t seed, std::initializer_list<glm::ivec2> trees)
{
    TileMap map{12, 12, seed, std::vector<Terrain>(144, Terrain::water), {}};
    for (glm::ivec2 hex : trees) {
        map.tiles[map.index(hex)] = Terrain::grass;
        map.trees.push_back({hex, 0, {}});
    }
    return map;
}

Tree& tree_at(TileMap& map, glm::ivec2 hex)
{
    const auto found = std::find_if(map.trees.begin(), map.trees.end(), [hex](const Tree& tree) {
        return tree.hex == hex;
    });
    test::expect(found != map.trees.end(), "Expected fuel tree is missing");
    return *found;
}

void advance(WorldSimulation& simulation, TileMap& map, std::uint64_t& tick,
             std::uint64_t until)
{
    while (tick < until) {
        tick += 8;
        simulation.update(map, tick);
    }
}

void check_fuel_and_burn_down()
{
    TileMap map = fuel_map(74129, {{-3, 0}, {0, 0}, {3, 0}});
    map.tiles[map.index({0, 0})] = Terrain::land;
    const auto original_terrain = map.tiles;
    WorldSimulation simulation;
    simulation.reset(map);
    tree_at(map, {-3, 0}).stage = TreeStage::growing;
    tree_at(map, {-3, 0}).age_ticks = 0;
    Tree& mature = tree_at(map, {0, 0});
    mature.age_ticks = mature.lifespan_ticks - 8;
    Tree& dead = tree_at(map, {3, 0});
    dead.stage = TreeStage::dead;
    dead.dead_ticks = dead.decay_ticks - 8;

    for (const Tree& tree : map.trees) {
        test::expect(simulation.queue_ignition(map, tree.id), "A fuel stage rejected ignition");
        test::expect(!simulation.queue_ignition(map, tree.id), "Duplicate ignition was queued twice");
        test::expect(!tree.burning(), "Queued ignition changed the frozen world immediately");
    }
    test::expect(simulation.pending_ignitions() == 3, "Pending command count is incorrect");
    test::expect(!simulation.queue_ignition(map, 0) &&
                 !simulation.queue_ignition(map, std::numeric_limits<std::uint64_t>::max()),
                 "Missing tree ID was accepted");

    std::uint64_t tick = 0;
    advance(simulation, map, tick, 8);
    test::expect(simulation.pending_ignitions() == 0, "Committed commands remained pending");
    std::uint64_t last_burnout = 0;
    for (const Tree& tree : map.trees) {
        test::expect(tree.burning() && tree.ignition_tick == 8 && !tree.charred,
                     "Ignition did not begin on the next world update");
        const auto duration = tree.burnout_tick - tree.ignition_tick;
        const bool dead_fuel = tree.stage == TreeStage::dead;
        test::expect(duration >= (dead_fuel ? 6 : 12) * 32 &&
                     duration <= (dead_fuel ? 10 : 20) * 32 && duration % 8 == 0,
                     "Burn duration is outside planned bounds");
        test::expect(!simulation.queue_ignition(map, tree.id), "Existing fire accepted a restart");
        last_burnout = std::max(last_burnout, tree.burnout_tick);
    }
    test::expect(tree_at(map, {0, 0}).stage == TreeStage::mature,
                 "Natural age death bypassed ignition");
    test::expect(tree_at(map, {3, 0}).burning(), "Corpse decay bypassed ignition");
    test::expect(map.tiles == original_terrain, "Ignition scorched terrain before burn-down");

    advance(simulation, map, tick, last_burnout);
    test::expect(map.trees.size() == 3, "Fuel disappeared before charred decay");
    for (const Tree& tree : map.trees) {
        const int cell = map.index(tree.hex);
        test::expect(tree.stage == TreeStage::dead && tree.charred && !tree.burning(),
                     "Burn-down did not leave inert charred remains");
        test::expect(tree.decay_ticks >= 15 * 32 && tree.decay_ticks <= 30 * 32,
                     "Charred-remains decay is outside planned bounds");
        test::expect(map.tiles[cell] == Terrain::burned &&
                     map.recovery[cell].original_terrain == original_terrain[cell],
                     "Burn-down did not preserve canonical original terrain");
        test::expect(map.recovery[cell].started_tick > tree.ignition_tick &&
                     map.recovery[cell].duration_ticks >= 90 * 32 &&
                     map.recovery[cell].duration_ticks <= 180 * 32,
                     "Burn-down did not initialize bounded recovery data");
        test::expect(!simulation.queue_ignition(map, tree.id), "Charred fuel was reignited");
    }
    const auto recovery = map.recovery;
    advance(simulation, map, tick, tick + 40 * 32);
    test::expect(map.trees.empty(), "Charred remains did not decay away");
    for (int cell = 0; cell < static_cast<int>(map.tiles.size()); ++cell) {
        test::expect(simulation.tree_index_at(cell) == -1, "Charred decay left stale occupancy");
        if (recovery[cell].duration_ticks == 0) {
            continue;
        }
        test::expect(map.tiles[cell] == Terrain::burned &&
                     map.recovery[cell].original_terrain == recovery[cell].original_terrain &&
                     map.recovery[cell].started_tick == recovery[cell].started_tick &&
                     map.recovery[cell].duration_ticks == recovery[cell].duration_ticks &&
                     map.recovery[cell].elapsed_ticks == recovery[cell].elapsed_ticks + 40 * 32,
                     "Burned terrain changed early or lost recovery progress");
    }
    simulation.reset(map);
    test::expect(map.tiles == original_terrain && simulation.pending_ignitions() == 0,
                 "Reset retained burned terrain or pending commands");
    test::expect(std::all_of(map.recovery.begin(), map.recovery.end(),
                 [](const BurnRecovery& state) { return state.duration_ticks == 0; }),
                 "Reset retained recovery records");
}

void check_staged_spread_and_gaps()
{
    bool observed_spread = false;
    for (std::uint32_t seed = 0; seed < 32 && !observed_spread; ++seed) {
        TileMap map = fuel_map(seed, {{0, 0}, {1, 0}, {2, 0}});
        WorldSimulation simulation;
        simulation.reset(map);
        simulation.queue_ignition(map, tree_at(map, {0, 0}).id);
        std::uint64_t tick = 0;
        advance(simulation, map, tick, 8);
        test::expect(!tree_at(map, {1, 0}).burning() && !tree_at(map, {2, 0}).burning(),
                     "New ignition spread during its own update");
        while (tree_at(map, {0, 0}).burning()) {
            advance(simulation, map, tick, tick + 8);
            if (tree_at(map, {1, 0}).burning()) {
                test::expect(!tree_at(map, {2, 0}).burning(),
                             "New spread recursively ignited another generation");
                observed_spread = true;
                break;
            }
        }
    }
    test::expect(observed_spread, "Reference seeds never spread to adjacent fuel");

    TileMap gap = fuel_map(42, {{0, 0}, {2, 0}});
    WorldSimulation simulation;
    simulation.reset(gap);
    simulation.queue_ignition(gap, tree_at(gap, {0, 0}).id);
    std::uint64_t tick = 0;
    advance(simulation, gap, tick, 8);
    while (tree_at(gap, {0, 0}).burning()) {
        advance(simulation, gap, tick, tick + 8);
        test::expect(!tree_at(gap, {2, 0}).burning(), "Fire jumped across an empty cell");
    }
}

void check_combined_pressure()
{
    int one_neighbor = 0;
    int two_neighbors = 0;
    for (std::uint32_t seed = 0; seed < 1024; ++seed) {
        TileMap one = fuel_map(seed, {{-1, 0}, {0, 0}, {1, 0}});
        TileMap two = one;
        WorldSimulation one_simulation;
        WorldSimulation two_simulation;
        one_simulation.reset(one);
        two_simulation.reset(two);
        one_simulation.queue_ignition(one, tree_at(one, {-1, 0}).id);
        two_simulation.queue_ignition(two, tree_at(two, {-1, 0}).id);
        two_simulation.queue_ignition(two, tree_at(two, {1, 0}).id);
        for (std::uint64_t tick : {8, 16}) {
            one_simulation.update(one, tick);
            two_simulation.update(two, tick);
        }
        const bool first = tree_at(one, {0, 0}).burning();
        const bool second = tree_at(two, {0, 0}).burning();
        test::expect(!first || second, "More burning neighbors reduced keyed target pressure");
        one_neighbor += first;
        two_neighbors += second;
    }
    test::expect(one_neighbor > 0 && two_neighbors > one_neighbor * 1.3,
                 "Combined burning neighbors do not increase spread pressure");
}

void expect_same_world(const TileMap& first, const TileMap& second)
{
    test::expect(first.tiles == second.tiles, "Vector order changed burned terrain");
    const auto first_trees = test::trees_by_cell(first);
    const auto second_trees = test::trees_by_cell(second);
    test::expect(first_trees.size() == second_trees.size(), "Vector order changed population");
    for (const auto& [cell, tree] : first_trees) {
        const Tree& other = second_trees.at(cell);
        test::expect_same_tree(tree, other);
        test::expect(tree.id == other.id && tree.stage == other.stage &&
                     tree.age_ticks == other.age_ticks && tree.dead_ticks == other.dead_ticks &&
                     tree.decay_ticks == other.decay_ticks && tree.charred == other.charred &&
                     tree.ignition_tick == other.ignition_tick && tree.burnout_tick == other.burnout_tick,
                     "Vector or command order changed fire/lifecycle state");
    }
    test::expect(first.recovery.size() == second.recovery.size(), "Recovery sizes differ");
    for (std::size_t cell = 0; cell < first.recovery.size(); ++cell) {
        const BurnRecovery& a = first.recovery[cell];
        const BurnRecovery& b = second.recovery[cell];
        test::expect(a.original_terrain == b.original_terrain && a.started_tick == b.started_tick &&
                     a.duration_ticks == b.duration_ticks && a.elapsed_ticks == b.elapsed_ticks,
                     "Vector order changed recovery records");
    }
}

void check_order_independence_and_reset()
{
    TileMap first{12, 12, 74129, std::vector<Terrain>(144, Terrain::grass), {}};
    for (int cell = 0; cell < static_cast<int>(first.tiles.size()); ++cell) {
        first.trees.push_back({first.hex_at(cell), static_cast<unsigned>(cell % 3), {}});
    }
    TileMap second = first;
    std::reverse(second.trees.begin(), second.trees.end());
    WorldSimulation first_simulation;
    WorldSimulation second_simulation;
    first_simulation.reset(first);
    second_simulation.reset(second);
    for (glm::ivec2 hex : {glm::ivec2{0, 0}, {1, 0}}) {
        first_simulation.queue_ignition(first, tree_at(first, hex).id);
    }
    for (glm::ivec2 hex : {glm::ivec2{1, 0}, {0, 0}}) {
        second_simulation.queue_ignition(second, tree_at(second, hex).id);
    }
    for (std::uint64_t tick = 8; tick <= 80 * 32; tick += 8) {
        first_simulation.update(first, tick);
        second_simulation.update(second, tick);
        if (tick % 32 == 0) {
            expect_same_world(first, second);
        }
    }
    test::expect(std::count(first.tiles.begin(), first.tiles.end(), Terrain::burned) > 2,
                 "Dense reference forest did not exercise staged spread");

    TileMap reset = fuel_map(7, {{0, 0}});
    WorldSimulation reset_simulation;
    reset_simulation.reset(reset);
    reset_simulation.queue_ignition(reset, reset.trees.front().id);
    reset_simulation.reset(reset);
    test::expect(reset_simulation.pending_ignitions() == 0 && !reset.trees.front().burning(),
                 "Reset retained a pending ignition");
    reset_simulation.update(reset, 8);
    test::expect(!reset.trees.front().burning(), "Pre-reset ignition reached a new world");
}

void check_ineligible_terrain_and_natural_death()
{
    TileMap map = fuel_map(3, {{0, 0}});
    WorldSimulation simulation;
    simulation.reset(map);
    for (Terrain terrain : {Terrain::water, Terrain::sand, Terrain::burned}) {
        map.tiles[map.index({0, 0})] = terrain;
        test::expect(!simulation.queue_ignition(map, map.trees.front().id),
                     "Fire accepted fuel on water, sand or already-burned terrain");
    }
    map.tiles[map.index({0, 0})] = Terrain::grass;
    map.trees.front().age_ticks = map.trees.front().lifespan_ticks - 8;
    simulation.update(map, 8);
    test::expect(map.trees.front().stage == TreeStage::dead &&
                 map.tiles[map.index({0, 0})] == Terrain::grass &&
                 map.recovery[map.index({0, 0})].duration_ticks == 0,
                 "Natural death scorched terrain");
}

} // namespace

int main()
{
    return test::run("Tree fire simulation", [] {
        check_fuel_and_burn_down();
        check_staged_spread_and_gaps();
        check_combined_pressure();
        check_order_independence_and_reset();
        check_ineligible_terrain_and_natural_death();
    });
}
