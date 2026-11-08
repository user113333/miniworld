#include "world_simulation.h"
#include "hex_grid.h"
#include "simulation_clock.h"
#include "tree_placement.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr std::uint64_t world_ticks = SimulationClock::ticks_per_world_step;
constexpr double world_seconds = world_ticks * SimulationClock::base_step_seconds;

enum class Sample : std::uint64_t {
    birth,
    variant,
    offset_x,
    offset_y,
    growth,
    lifespan,
    decay,
    initial_age,
    spread,
    burn_duration,
    charred_decay,
    recovery_duration
};

std::uint64_t mix(std::uint64_t value)
{
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

std::uint64_t sample(std::uint32_t seed, std::uint64_t tick,
                     std::uint64_t identity, Sample channel)
{
    // This keyed stream is separate from initial terrain/forest generation.
    return mix(mix(seed ^ 0x54ce64d9e8c1c3b7ULL) ^ mix(tick)
               ^ mix(identity + 0x9e3779b97f4a7c15ULL)
               ^ mix(static_cast<std::uint64_t>(channel) + 0xd1b54a32d192ed03ULL));
}

double unit_sample(std::uint32_t seed, std::uint64_t tick,
                   std::uint64_t identity, Sample channel)
{
    return static_cast<double>(sample(seed, tick, identity, channel) >> 11)
           / 9007199254740992.0;
}

std::uint64_t duration(std::uint32_t seed, std::uint64_t identity, Sample channel,
                       double minimum_seconds, double maximum_seconds)
{
    const auto minimum = static_cast<std::uint64_t>(minimum_seconds / world_seconds);
    const auto maximum = static_cast<std::uint64_t>(maximum_seconds / world_seconds);
    return (minimum + sample(seed, 0, identity, channel) % (maximum - minimum + 1)) * world_ticks;
}

bool can_grow(Terrain terrain)
{
    return terrain == Terrain::grass || terrain == Terrain::land;
}

bool susceptible(const TileMap& map, const Tree& tree)
{
    return !tree.charred && !tree.burning() && map.contains(tree.hex)
           && can_grow(map.tiles[map.index(tree.hex)]);
}

} // namespace

void WorldSimulation::rebuild_occupancy(const TileMap& map)
{
    occupancy_.assign(map.tiles.size(), -1);
    for (std::size_t index = 0; index < map.trees.size(); ++index) {
        const Tree& tree = map.trees[index];
        if (!map.contains(tree.hex)) {
            throw std::invalid_argument("Tree lies outside its world");
        }
        const int cell = map.index(tree.hex);
        if (occupancy_[cell] != -1) {
            throw std::invalid_argument("Multiple trees occupy one cell");
        }
        occupancy_[cell] = static_cast<int>(index);
    }
}

void WorldSimulation::initialize_lifecycle(Tree& tree, std::uint32_t seed) const
{
    tree.age_ticks = 0;
    tree.dead_ticks = 0;
    tree.ignition_tick = 0;
    tree.burnout_tick = 0;
    tree.charred = false;
    tree.growth_ticks = duration(seed, tree.id, Sample::growth,
                                 ForestTuning::growth_seconds_min, ForestTuning::growth_seconds_max);
    tree.lifespan_ticks = duration(seed, tree.id, Sample::lifespan,
                                   ForestTuning::lifespan_seconds_min, ForestTuning::lifespan_seconds_max);
    tree.decay_ticks = duration(seed, tree.id, Sample::decay,
                                ForestTuning::decay_seconds_min, ForestTuning::decay_seconds_max);
}

void WorldSimulation::reset(TileMap& map)
{
    last_tick_ = 0;
    next_tree_id_ = 1;
    births_.clear();
    ignitions_.clear();
    pending_ignitions_.clear();
    for (std::size_t cell = 0; cell < map.tiles.size() && cell < map.recovery.size(); ++cell) {
        const BurnRecovery& recovery = map.recovery[cell];
        if (map.tiles[cell] == Terrain::burned && recovery.duration_ticks != 0) {
            map.tiles[cell] = recovery.original_terrain;
        }
    }
    map.recovery.assign(map.tiles.size(), {});
    rebuild_occupancy(map);
    for (int index : occupancy_) {
        if (index < 0) {
            continue;
        }
        Tree& tree = map.trees[index];
        tree.id = next_tree_id_++;
        tree.stage = TreeStage::mature;
        initialize_lifecycle(tree, map.seed);
        // Seeded mature ages leave at least 40% of each lifespan ahead. Keeping
        // ages on world boundaries prevents a synchronized first generation.
        const auto minimum_age = tree.growth_ticks / world_ticks;
        const auto maximum_age = tree.lifespan_ticks * 3 / (5 * world_ticks);
        tree.age_ticks = (minimum_age + sample(map.seed, 0, tree.id, Sample::initial_age)
                         % (maximum_age - minimum_age + 1)) * world_ticks;
    }
}

Tree WorldSimulation::make_tree(const TileMap& map, int cell, std::uint64_t tick)
{
    Tree tree{map.hex_at(cell),
              static_cast<unsigned>(sample(map.seed, tick, cell, Sample::variant) % 3),
              glm::vec2{static_cast<float>(unit_sample(map.seed, tick, cell, Sample::offset_x)),
                        static_cast<float>(unit_sample(map.seed, tick, cell, Sample::offset_y))}};
    // As with initial trees, this square fits inside the owning hex.
    tree.offset = (tree.offset - 0.5f) * 0.8f;
    tree.id = next_tree_id_++;
    tree.stage = TreeStage::growing;
    initialize_lifecycle(tree, map.seed);
    return tree;
}

bool WorldSimulation::queue_ignition(const TileMap& map, std::uint64_t tree_id)
{
    const auto found = std::find_if(map.trees.begin(), map.trees.end(), [tree_id](const Tree& tree) {
        return tree.id == tree_id;
    });
    if (tree_id == 0 || found == map.trees.end() || !susceptible(map, *found) ||
        pending_ignitions_.size() >= map.trees.size() ||
        std::find(pending_ignitions_.begin(), pending_ignitions_.end(), tree_id) != pending_ignitions_.end()) {
        return false;
    }
    pending_ignitions_.push_back(tree_id);
    return true;
}

void WorldSimulation::collect_ignitions(const TileMap& map, std::uint64_t tick)
{
    ignitions_.clear();
    if (pending_ignitions_.empty() && std::none_of(map.trees.begin(), map.trees.end(),
        [](const Tree& tree) { return tree.burning(); })) {
        return;
    }
    std::sort(pending_ignitions_.begin(), pending_ignitions_.end());
    for (int cell = 0; cell < static_cast<int>(occupancy_.size()); ++cell) {
        const int index = occupancy_[cell];
        if (index < 0 || !susceptible(map, map.trees[index])) {
            continue;
        }
        const Tree& target = map.trees[index];
        if (std::binary_search(pending_ignitions_.begin(), pending_ignitions_.end(), target.id)) {
            ignitions_.push_back(cell);
            continue;
        }
        int burning_neighbors = 0;
        for (glm::ivec2 direction : hex_neighbors) {
            const glm::ivec2 neighbor = target.hex + direction;
            if (!map.contains(neighbor)) {
                continue;
            }
            const int neighbor_index = occupancy_[map.index(neighbor)];
            burning_neighbors += neighbor_index >= 0 && map.trees[neighbor_index].burning();
        }
        if (burning_neighbors == 0) {
            continue;
        }
        const double probability = -std::expm1(-FireTuning::spread_rate * burning_neighbors * world_seconds);
        if (unit_sample(map.seed, tick, target.id, Sample::spread) < probability) {
            ignitions_.push_back(cell);
        }
    }
    pending_ignitions_.clear();
}

void WorldSimulation::ignite(Tree& tree, std::uint32_t seed, std::uint64_t tick) const
{
    const bool dead = tree.stage == TreeStage::dead;
    const auto burn_ticks = duration(seed, tree.id, Sample::burn_duration,
        dead ? FireTuning::dead_burn_seconds_min : FireTuning::burn_seconds_min,
        dead ? FireTuning::dead_burn_seconds_max : FireTuning::burn_seconds_max);
    tree.ignition_tick = tick;
    tree.burnout_tick = tick + burn_ticks;
}

void WorldSimulation::burn_down(TileMap& map, Tree& tree, std::uint64_t tick) const
{
    tree.stage = TreeStage::dead;
    tree.charred = true;
    tree.burnout_tick = 0;
    tree.dead_ticks = 0;
    tree.decay_ticks = duration(map.seed, tree.id, Sample::charred_decay,
                               FireTuning::charred_decay_seconds_min, FireTuning::charred_decay_seconds_max);
    const int cell = map.index(tree.hex);
    if (can_grow(map.tiles[cell])) {
        map.recovery[cell] = {map.tiles[cell], tick,
            duration(map.seed, cell, Sample::recovery_duration,
                     FireTuning::recovery_seconds_min, FireTuning::recovery_seconds_max)};
        map.tiles[cell] = Terrain::burned;
    }
}

void WorldSimulation::advance_recovery(TileMap& map) const
{
    for (std::size_t cell = 0; cell < map.recovery.size(); ++cell) {
        BurnRecovery& recovery = map.recovery[cell];
        if (recovery.duration_ticks == 0) {
            continue;
        }
        recovery.elapsed_ticks = std::min(recovery.duration_ticks,
                                         recovery.elapsed_ticks + world_ticks);
        if (recovery.elapsed_ticks == recovery.duration_ticks) {
            map.tiles[cell] = recovery.original_terrain;
            recovery = {};
        }
    }
}

void WorldSimulation::update(TileMap& map, std::uint64_t tick)
{
    if (tick != last_tick_ + world_ticks) {
        throw std::invalid_argument("World updates require successive eight-tick boundaries");
    }
    rebuild_occupancy(map);
    collect_ignitions(map, tick);
    births_.clear();

    // Birth intents see the start-of-step occupancy, including corpses that
    // decay below. A cell cleared this step cannot immediately receive a tree.
    for (int cell = 0; cell < static_cast<int>(map.tiles.size()); ++cell) {
        const Terrain terrain = map.tiles[cell];
        if (occupancy_[cell] >= 0 || !can_grow(terrain)) {
            continue;
        }
        const double rate = terrain == Terrain::grass ? ForestTuning::grass_birth_rate
                                                      : ForestTuning::land_birth_rate;
        const double suitability = forest_suitability(map.hex_at(cell), map.seed);
        // Strong habitat preference keeps established patches dense while
        // leaving a small chance to colonize less suitable open ground.
        const double habitat = ForestTuning::habitat_birth_scale
                               * suitability * suitability * suitability;
        const double probability = -std::expm1(-rate * habitat * world_seconds);
        if (unit_sample(map.seed, tick, cell, Sample::birth) < probability) {
            births_.push_back(cell);
        }
    }

    // Births saw each cell's old terrain above. Restore existing scars before
    // lifecycle updates create fresh ones, which must start at zero progress.
    advance_recovery(map);

    for (int cell : ignitions_) {
        ignite(map.trees[occupancy_[cell]], map.seed, tick);
    }

    for (Tree& tree : map.trees) {
        if (tree.burning()) {
            if (tick >= tree.burnout_tick) {
                burn_down(map, tree, tick);
            } else if (tree.stage != TreeStage::dead && tree.ignition_tick != tick) {
                tree.age_ticks += world_ticks;
                if (tree.stage == TreeStage::growing && tree.age_ticks >= tree.growth_ticks) {
                    tree.stage = TreeStage::mature;
                }
            }
            continue;
        }
        if (tree.stage == TreeStage::dead) {
            tree.dead_ticks += world_ticks;
            continue;
        }
        tree.age_ticks += world_ticks;
        if (tree.age_ticks >= tree.lifespan_ticks) {
            tree.stage = TreeStage::dead;
            tree.dead_ticks = 0;
        } else if (tree.stage == TreeStage::growing && tree.age_ticks >= tree.growth_ticks) {
            tree.stage = TreeStage::mature;
        }
    }

    map.trees.erase(std::remove_if(map.trees.begin(), map.trees.end(), [](const Tree& tree) {
        return !tree.burning() && tree.stage == TreeStage::dead && tree.dead_ticks >= tree.decay_ticks;
    }), map.trees.end());
    for (int cell : births_) {
        map.trees.push_back(make_tree(map, cell, tick));
    }
    rebuild_occupancy(map);
    last_tick_ = tick;
}

int WorldSimulation::tree_index_at(int cell) const
{
    if (cell < 0 || static_cast<std::size_t>(cell) >= occupancy_.size()) {
        return -1;
    }
    return occupancy_[cell];
}
