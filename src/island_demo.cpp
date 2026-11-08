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
}

void IslandDemo::update_trees()
{
    place_trees(map_, settings.tree_density);
}
