#pragma once

#include "tile_map.h"

void place_trees(TileMap& map, float density);

// The initial forest fills low-noise patches; later births prefer those same patches.
float forest_suitability(glm::ivec2 hex, std::uint32_t seed);
