#pragma once

#include "tile_map.h"
#include "simulation_clock.h"

#include <random>

struct IslandSettings {
    int width = 48;
    int height = 32;
    std::uint32_t seed = 0;
    float tree_density = 0.20f;
    float simulation_speed = 1.0f;
};

class IslandDemo {
public:
    IslandDemo();
    void generate();
    void regenerate();
    void update_trees();
    void advance_simulation(double real_seconds, bool active = true);
    void toggle_pause();
    const TileMap& map() const { return map_; }
    const SimulationClock& clock() const { return clock_; }
    std::uint64_t world_generation() const { return world_generation_; }

    IslandSettings settings;

private:
    std::mt19937 random_{std::random_device{}()};
    TileMap map_;
    SimulationClock clock_;
    std::uint64_t world_generation_ = 0;
    float resume_speed_ = 1.0f;
};
