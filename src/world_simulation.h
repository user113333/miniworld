#pragma once

#include "tile_map.h"

#include <cstdint>
#include <vector>

// Durations are simulation seconds; birth rates are per empty cell per second.
struct ForestTuning {
    static constexpr double growth_seconds_min = 15.0;
    static constexpr double growth_seconds_max = 30.0;
    static constexpr double lifespan_seconds_min = 180.0;
    static constexpr double lifespan_seconds_max = 360.0;
    static constexpr double decay_seconds_min = 30.0;
    static constexpr double decay_seconds_max = 60.0;
    static constexpr double grass_birth_rate = 0.002;
    static constexpr double land_birth_rate = 0.0005;
    static constexpr double habitat_birth_scale = 16.0;
};

class WorldSimulation {
public:
    // Initialize runtime state without changing the approved initial tree layout.
    // Call again whenever the world is replaced or initial trees are placed.
    void reset(TileMap& map);

    // Advance exactly one world step at successive eight-base-tick boundaries.
    void update(TileMap& map, std::uint64_t tick);

    // Indices are valid only until the next reset/update. Empty/outside is -1.
    int tree_index_at(int cell) const;

private:
    void rebuild_occupancy(const TileMap& map);
    Tree make_tree(const TileMap& map, int cell, std::uint64_t tick);
    void initialize_lifecycle(Tree& tree, std::uint32_t seed) const;

    std::vector<int> occupancy_;
    std::vector<int> births_;
    std::uint64_t next_tree_id_ = 1;
    std::uint64_t last_tick_ = 0;
};
