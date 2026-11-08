#pragma once

#include "particle_system.h"
#include "tile_map.h"

// Adapter from authoritative trees to read-only visual sources.
void collect_fire_emitters(const TileMap& map, std::vector<FireEmitter>& emitters);
