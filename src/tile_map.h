#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

enum class Terrain { water, sand, land, grass };

struct TileMap {
    int width;
    int height;
    std::uint32_t seed;
    std::vector<Terrain> tiles;

    // Storage is a rectangular odd-row offset grid; world coordinates are axial.
    glm::ivec2 hex_at(int index) const
    {
        const int row = index / width;
        return {index % width - row / 2, row};
    }

    bool contains(glm::ivec2 hex) const
    {
        const int column = hex.x + hex.y / 2;
        return hex.y >= 0 && hex.y < height && column >= 0 && column < width;
    }

    int index(glm::ivec2 hex) const
    {
        return hex.y * width + hex.x + hex.y / 2;
    }
};
