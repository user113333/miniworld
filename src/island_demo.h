#pragma once

#include "tile_map.h"

#include <random>

struct IslandSettings {
    int width = 48;
    int height = 32;
    std::uint32_t seed = 0;
};

class IslandDemo {
public:
    IslandDemo();
    void generate();
    void regenerate();
    const TileMap& map() const { return map_; }

    IslandSettings settings;

private:
    std::mt19937 random_{std::random_device{}()};
    TileMap map_;
};
