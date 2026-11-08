#include "render/particle_renderer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {
float logical_pixel_boundary(float physical, float scale)
{
    float logical = physical / scale;
    // SDL's software geometry path truncates scaled vertices. Round-trip error
    // just below an integer would otherwise remove a physical edge pixel.
    if (physical >= 0.0f && logical * scale < physical) {
        logical = std::nextafter(logical, std::numeric_limits<float>::infinity());
    } else if (physical < 0.0f && logical * scale > physical) {
        logical = std::nextafter(logical, -std::numeric_limits<float>::infinity());
    }
    return logical;
}

float logical_pixel_end(float physical, float side, float minimum, float scale)
{
    float logical = logical_pixel_boundary(physical, scale);
    // SDL may turn a quad into a fill rectangle, so its width must also survive
    // subtraction and scaling without truncating a fractional edge pixel.
    while ((logical - minimum) * scale < side) {
        logical = std::nextafter(logical, std::numeric_limits<float>::infinity());
    }
    return logical;
}
} // namespace

void ParticleRenderer::prepare(const ParticleSystem& system, const ActorSceneView& view)
{
    particles_ = system.data();
    ordered_.resize(system.size());
    std::iota(ordered_.begin(), ordered_.end(), 0);
    std::sort(ordered_.begin(), ordered_.end(), [this](std::size_t a, std::size_t b) {
        const Particle& first = particles_[a];
        const Particle& second = particles_[b];
        if (first.emitter_id != second.emitter_id) {
            return first.emitter_id < second.emitter_id;
        }
        if (first.kind != second.kind) {
            return first.kind < second.kind;
        }
        return first.birth_ordinal < second.birth_ordinal;
    });
    groups_.clear();
    for (std::size_t first = 0; first < ordered_.size();) {
        const Particle& particle = particles_[ordered_[first]];
        std::size_t last = first + 1;
        while (last < ordered_.size() && particles_[ordered_[last]].emitter_id == particle.emitter_id) {
            ++last;
        }
        groups_.push_back({particle.emitter_id, snapped_actor_ground(particle.ground, view), first, last});
        first = last;
    }
}

void ParticleRenderer::draw(SDL_Renderer* renderer, const Group& group, ParticleKind kind,
                             const ActorSceneView& view)
{
    const float pixel_scale = actor_source_pixel_scale(view);
    if (pixel_scale <= 0.0f) {
        return;
    }
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height)) {
        SDL_Log("Cannot read particle output size: %s", SDL_GetError());
        return;
    }
    vertices_.clear();
    indices_.clear();
    for (std::size_t position = group.first; position < group.last; ++position) {
        const Particle& particle = particles_[ordered_[position]];
        if (particle.kind != kind) {
            continue;
        }
        const float side = glm::max(1.0f, glm::round(particle.size * pixel_scale));
        const glm::vec2 physical = glm::round((view.origin + particle.position * view.radius)
                                              * view.framebuffer_scale - side * 0.5f);
        if (physical.x + side <= 0 || physical.y + side <= 0 ||
            physical.x >= output_width || physical.y >= output_height) {
            continue;
        }
        const glm::vec2 minimum{
            logical_pixel_boundary(physical.x, view.framebuffer_scale.x),
            logical_pixel_boundary(physical.y, view.framebuffer_scale.y)};
        const glm::vec2 maximum{
            logical_pixel_end(physical.x + side, side, minimum.x, view.framebuffer_scale.x),
            logical_pixel_end(physical.y + side, side, minimum.y, view.framebuffer_scale.y)};
        const SDL_FColor color{particle.color.r, particle.color.g, particle.color.b, particle.color.a};
        const int first = static_cast<int>(vertices_.size());
        vertices_.insert(vertices_.end(), {
            {{minimum.x, minimum.y}, color, {0, 0}},
            {{maximum.x, minimum.y}, color, {0, 0}},
            {{maximum.x, maximum.y}, color, {0, 0}},
            {{minimum.x, maximum.y}, color, {0, 0}}
        });
        indices_.insert(indices_.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
    }
    if (vertices_.empty()) {
        return;
    }
    SDL_BlendMode previous_blend;
    if (!SDL_GetRenderDrawBlendMode(renderer, &previous_blend)) {
        SDL_Log("Cannot read particle blend mode: %s", SDL_GetError());
        return;
    }
    if (!SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) ||
        !SDL_RenderGeometry(renderer, nullptr, vertices_.data(), static_cast<int>(vertices_.size()),
                            indices_.data(), static_cast<int>(indices_.size()))) {
        SDL_Log("Cannot draw fire particles: %s", SDL_GetError());
    }
    if (!SDL_SetRenderDrawBlendMode(renderer, previous_blend)) {
        SDL_Log("Cannot restore particle blend mode: %s", SDL_GetError());
    }
}
