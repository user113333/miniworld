#include "simulation.h"
#include "world/island_generator.h"
#include "world/tree_placement.h"

Simulation::Simulation() { generate(); }

void Simulation::generate()
{
    island_settings.seed = random_();
    regenerate();
}

void Simulation::regenerate()
{
    auto map = generate_island(island_settings.width, island_settings.height, island_settings.seed);
    const auto trees = place_trees(map, island_settings.tree_density);
    world_.reset(std::move(map), trees);
    clock_.reset();
}

void Simulation::advance_simulation(double real_seconds, bool active)
{
    accumulate_simulation_time(real_seconds, active);
    while (step_simulation()) {}
}

void Simulation::accumulate_simulation_time(double real_seconds, bool active)
{
    // Commit UI commands at the frame boundary, even when paused, without a clock step.
    world_.flush_edits();
    clock_.accumulate(real_seconds, settings.speed, active);
}

bool Simulation::step_simulation()
{
    if (!clock_.step()) return false;
    if (clock_.world_update_due()) world_.update(clock_.ticks());
    return true;
}

void Simulation::toggle_pause()
{
    if (settings.speed > 0.0f) {
        resume_speed_ = settings.speed;
        settings.speed = 0.0f;
    } else {
        settings.speed = resume_speed_;
    }
}
