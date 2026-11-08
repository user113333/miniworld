#pragma once

#include "render/map_renderer.h"
#include "effects/particle_system.h"
#include "render/actor_scene.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class ParticleRenderer {
public:
    struct Group {
        std::uint64_t emitter_id;
        glm::vec2 ground;
        std::size_t first;
        std::size_t last;
    };

    // One grouping pass per frame; drawing each group only visits its particles.
    void prepare(const ParticleSystem& system, const ActorSceneView& view);
    const std::vector<Group>& groups() const { return groups_; }
    void draw(SDL_Renderer* renderer, const Group& group, ParticleKind kind,
              const ActorSceneView& view);

private:
    const Particle* particles_ = nullptr;
    std::vector<std::size_t> ordered_;
    std::vector<Group> groups_;
    std::vector<SDL_Vertex> vertices_;
    std::vector<int> indices_;
};
