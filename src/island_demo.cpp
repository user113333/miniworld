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
    update_trees();
    clock_.reset();
    ++world_generation_;
}

void IslandDemo::update_trees()
{
    place_trees(map_, settings.tree_density);
}

void IslandDemo::advance_simulation(double real_seconds, bool active)
{
    clock_.accumulate(real_seconds, settings.simulation_speed, active);
    while (clock_.step()) {
        // Later phases add world updates every eight ticks, then effects each tick.
    }
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
