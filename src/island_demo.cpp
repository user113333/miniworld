#include "island_demo.h"
#include "island_generator.h"
#include "tree_placement.h"

IslandDemo::IslandDemo()
{
    generate();
}

void IslandDemo::generate()
{
    settings.seed = random_();
    regenerate();
}

void IslandDemo::regenerate()
{
    map_ = generate_island(settings.width, settings.height, settings.seed);
    place_trees(map_, settings.tree_density);
    simulation_.reset(map_);
    clock_.reset();
    settings.select_entities = false;
    settings.ignite_trees = false;
    ++world_generation_;
}

void IslandDemo::advance_simulation(double real_seconds, bool active)
{
    accumulate_simulation_time(real_seconds, active);
    while (step_simulation()) {
    }
}

void IslandDemo::accumulate_simulation_time(double real_seconds, bool active)
{
    clock_.accumulate(real_seconds, settings.simulation_speed, active);
}

bool IslandDemo::step_simulation()
{
    if (!clock_.step()) {
        return false;
    }
    if (clock_.world_update_due()) {
        simulation_.update(map_, clock_.ticks());
    }
    return true;
}

bool IslandDemo::queue_ignition(std::uint64_t tree_id, std::uint64_t generation)
{
    return generation == world_generation_ && simulation_.queue_ignition(map_, tree_id);
}

void IslandDemo::toggle_pause()
{
    if (settings.simulation_speed > 0.0f) {
        resume_speed_ = settings.simulation_speed;
        settings.simulation_speed = 0.0f;
    } else {
        settings.simulation_speed = resume_speed_;
    }
}
