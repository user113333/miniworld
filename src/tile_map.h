#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

enum class Terrain { water, sand, land, grass };

struct Tree {
    glm::ivec2 hex;
    unsigned int variant;
    glm::vec2 offset{}; // Ground offset in hex-radius units, inside the owning tile.
};

struct TileMap {
    int width;
    int height;
    std::uint32_t seed;
    std::vector<Terrain> tiles;
    std::vector<Tree> trees;

    // World (0, 0) is the middle storage tile, not the upper-left corner.
    glm::ivec2 storage_origin() const
    {
        const int row = height / 2;
        return {width / 2 - row / 2, row};
    }

    // Storage is a rectangular odd-row offset grid; world coordinates are axial.
    glm::ivec2 hex_at(int index) const
    {
        const int row = index / width;
        return glm::ivec2{index % width - row / 2, row} - storage_origin();
    }

    bool contains(glm::ivec2 hex) const
    {
        const glm::ivec2 storage = hex + storage_origin();
        const int column = storage.x + storage.y / 2;
        return storage.y >= 0 && storage.y < height && column >= 0 && column < width;
    }

    int index(glm::ivec2 hex) const
    {
        const glm::ivec2 storage = hex + storage_origin();
        return storage.y * width + storage.x + storage.y / 2;
    }
};
