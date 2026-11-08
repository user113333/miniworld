#pragma once

#include "map_renderer.h"
#include "particle_renderer.h"
#include "tree_scene.h"

#include <array>

class TreeRenderer {
public:
    TreeRenderer() = default;
    ~TreeRenderer();
    TreeRenderer(const TreeRenderer&) = delete;
    TreeRenderer& operator=(const TreeRenderer&) = delete;

    bool initialize(SDL_Renderer* renderer);
    void draw(SDL_Renderer* renderer, const TileMap& map, const MapView& view,
              const ParticleSystem* particles = nullptr) const;
    std::uint64_t pick(const TileMap& map, const MapView& view, glm::vec2 framebuffer_scale,
                       glm::vec2 logical_mouse) const;
    void shutdown();

private:
    struct Sprite {
        SDL_Texture* texture = nullptr;
    };
    struct DrawEntry {
        std::size_t tree;
        std::size_t particle_group;
        glm::vec2 ground;
        unsigned variant;
        std::uint64_t id;
    };
    std::array<Sprite, 5> sprites_{};
    std::array<TreeSpriteMask, 5> masks_{};
    mutable std::vector<TreeSceneEntry> scene_;
    mutable std::vector<DrawEntry> draw_entries_;
    mutable std::vector<bool> grouped_;
    mutable ParticleRenderer particle_renderer_;
};
