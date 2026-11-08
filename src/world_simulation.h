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

struct FireTuning {
    static constexpr double burn_seconds_min = 12.0;
    static constexpr double burn_seconds_max = 20.0;
    static constexpr double dead_burn_seconds_min = 6.0;
    static constexpr double dead_burn_seconds_max = 10.0;
    static constexpr double charred_decay_seconds_min = 15.0;
    static constexpr double charred_decay_seconds_max = 30.0;
    static constexpr double spread_rate = 0.15;
    static constexpr double recovery_seconds_min = 90.0;
    static constexpr double recovery_seconds_max = 180.0;
};

class WorldSimulation {
public:
    // Initialize runtime state without changing the approved initial tree layout.
    // Call again whenever the world is replaced or initial trees are placed.
    void reset(TileMap& map);

    // Advance exactly one world step at successive eight-base-tick boundaries.
    void update(TileMap& map, std::uint64_t tick);

    // Commands wait for the next world update, including while paused.
    bool queue_ignition(const TileMap& map, std::uint64_t tree_id);
    std::size_t pending_ignitions() const { return pending_ignitions_.size(); }

    // Indices are valid only until the next reset/update. Empty/outside is -1.
    int tree_index_at(int cell) const;

private:
    void rebuild_occupancy(const TileMap& map);
    Tree make_tree(const TileMap& map, int cell, std::uint64_t tick);
    void initialize_lifecycle(Tree& tree, std::uint32_t seed) const;
    void collect_ignitions(const TileMap& map, std::uint64_t tick);
    void ignite(Tree& tree, std::uint32_t seed, std::uint64_t tick) const;
    void burn_down(TileMap& map, Tree& tree, std::uint64_t tick) const;
    void advance_recovery(TileMap& map) const;

    std::vector<int> occupancy_;
    std::vector<int> births_;
    std::vector<int> ignitions_;
    std::vector<std::uint64_t> pending_ignitions_;
    std::uint64_t next_tree_id_ = 1;
    std::uint64_t last_tick_ = 0;
};
