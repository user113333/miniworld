#pragma once

#include "simulation_clock.h"
#include "world.h"
#include <random>

struct IslandSettings {
    int width = 48;
    int height = 32;
    std::uint32_t seed = 0;
    float tree_density = 0.20f;
};
struct SimulationSettings { float speed = 1.0f; };

class Simulation {
public:
    Simulation();
    void generate();
    void regenerate();
    void advance_simulation(double real_seconds, bool active = true);
    void accumulate_simulation_time(double real_seconds, bool active = true);
    bool step_simulation();
    bool enqueue(IgniteActor command) { return world_.enqueue(command); }
    std::uint64_t submit_edit(EntityEdit edit) { return world_.submit_edit(std::move(edit)); }
    std::size_t pending_ignitions() const { return world_.pending_ignitions(); }
    void toggle_pause();
    World& world() { return world_; }
    const World& world() const { return world_; }
    const TileMap& map() const { return world_.map(); }
    const SimulationClock& clock() const { return clock_; }
    std::uint64_t world_generation() const { return world_.generation(); }

    IslandSettings island_settings;
    SimulationSettings settings;

private:
    std::mt19937 random_{std::random_device{}()};
    World world_;
    SimulationClock clock_;
    float resume_speed_ = 1.0f;
};
