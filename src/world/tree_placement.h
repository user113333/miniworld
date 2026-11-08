#pragma once

#include "world/tile_map.h"
#include "actors/tree.h"

std::vector<TreeSpawn> place_trees(const TileMap& map, float density);

// The initial forest fills low-noise patches; later births prefer those same patches.
float forest_suitability(glm::ivec2 hex, std::uint32_t seed);
