#pragma once

#include "map_renderer.h"

#include <array>

class TreeRenderer {
public:
    TreeRenderer() = default;
    ~TreeRenderer();
    TreeRenderer(const TreeRenderer&) = delete;
    TreeRenderer& operator=(const TreeRenderer&) = delete;

    bool initialize(SDL_Renderer* renderer);
    void draw(SDL_Renderer* renderer, const TileMap& map, const MapView& view) const;
    void shutdown();

private:
    struct Sprite {
        SDL_Texture* texture = nullptr;
        glm::ivec2 size{};
    };
    std::array<Sprite, 5> sprites_{};
};
