#include "island_demo.h"
#include "island_generator.h"

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
}
