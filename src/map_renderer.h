#pragma once

#include "tile_map.h"

#include <SDL3/SDL.h>

struct MapView {
    glm::vec2 origin;
    float radius;
};

SDL_FColor terrain_color(Terrain terrain, bool weathered = false);
MapView fit_map(const TileMap& map, const SDL_FRect& area);
void draw_map(SDL_Renderer* renderer, const TileMap& map, const MapView& view);
