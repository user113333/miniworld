#pragma once

#include "render/map_renderer.h"
#include "render/particle_renderer.h"
#include "render/actor_scene.h"

#include <array>

class ActorRenderer {
public:
    ActorRenderer() = default;
    ~ActorRenderer();
    ActorRenderer(const ActorRenderer&) = delete;
    ActorRenderer& operator=(const ActorRenderer&) = delete;

    bool initialize(SDL_Renderer* renderer);
    void draw(SDL_Renderer* renderer, const TileMap& map, const std::vector<ActorView>& actors,
              const MapView& view,
              const ParticleSystem* particles = nullptr) const;
    ActorHandle pick(const TileMap& map, const std::vector<ActorView>& actors,
              const MapView& view, glm::vec2 framebuffer_scale,
                       glm::vec2 logical_mouse) const;
    void shutdown();

private:
    struct Sprite {
        SDL_Texture* texture = nullptr;
    };
    struct DrawEntry {
        std::size_t actor;
        std::size_t particle_group;
        glm::vec2 ground;
        unsigned variant;
        std::uint64_t id;
    };
    std::array<Sprite, 5> sprites_{};
    std::array<ActorSpriteMask, 5> masks_{};
    mutable std::vector<ActorSceneEntry> scene_;
    mutable std::vector<DrawEntry> draw_entries_;
    mutable std::vector<bool> grouped_;
    mutable ParticleRenderer particle_renderer_;
};
