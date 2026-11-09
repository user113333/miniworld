#pragma once
#include "tile_map.h"
#include "hex_grid.h"

inline int hex_distance(glm::ivec2 a, glm::ivec2 b)
{
    const auto d = a - b;
    return (glm::abs(d.x) + glm::abs(d.y) + glm::abs(d.x + d.y)) / 2;
}
bool walkable(const TileMap& map, glm::ivec2 hex);
glm::ivec2 world_hex(glm::vec2 position);
// At most 331 cells (radius ten); an empty result means unreachable.
std::vector<glm::ivec2> find_route(const TileMap& map, glm::ivec2 start,
                                 glm::ivec2 goal, int radius = 10);
