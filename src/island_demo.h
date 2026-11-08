#pragma once

#include "tile_map.h"

#include <random>

struct SDL_Renderer;

class IslandDemo {
public:
    IslandDemo();
    void draw_controls();
    void draw(SDL_Renderer* renderer) const;

private:
    std::mt19937 random_{std::random_device{}()};
    int width_ = 48;
    int height_ = 32;
    std::uint32_t seed_;
    TileMap map_;
};
