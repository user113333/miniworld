#include "tree_placement.h"

namespace {
std::uint32_t tree_hash(glm::ivec2 hex, std::uint32_t seed)
{
    std::uint32_t hash = seed ^ 0x51ed270bu
        ^ (static_cast<std::uint32_t>(hex.x) * 0x9e3779b9u)
        ^ (static_cast<std::uint32_t>(hex.y) * 0x85ebca6bu);
    hash = (hash ^ (hash >> 16)) * 0x7feb352du;
    hash = (hash ^ (hash >> 15)) * 0x846ca68bu;
    return hash ^ (hash >> 16);
}
} // namespace

void place_trees(TileMap& map, float density)
{
    map.trees.clear();
    density = glm::clamp(density, 0.0f, 1.0f);
    for (int index = 0; index < static_cast<int>(map.tiles.size()); ++index) {
        const Terrain terrain = map.tiles[index];
        if (terrain != Terrain::grass && terrain != Terrain::land) {
            continue;
        }
        const glm::ivec2 hex = map.hex_at(index);
        const std::uint32_t hash = tree_hash(hex, map.seed);
        const float chance = static_cast<float>(hash & 0xffffffu) / 16777216.0f;
        if (chance < density) {
            map.trees.push_back({hex, (hash >> 24) % 3});
        }
    }
}
