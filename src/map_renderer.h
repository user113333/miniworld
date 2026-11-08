#pragma once

#include "tile_map.h"

#include <SDL3/SDL.h>

void draw_map(SDL_Renderer* renderer, const TileMap& map, const SDL_FRect& area);
