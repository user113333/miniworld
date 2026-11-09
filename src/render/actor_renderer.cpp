#include "render/actor_renderer.h"

#include <algorithm>
#include <limits>
#include <string>

namespace {
constexpr std::array<const char*, 5> sprite_names{
    "tree1.png", "tree2.png", "tree3.png", "tree_growing.png", "tree_dead.png"
};
constexpr std::size_t no_entry = std::numeric_limits<std::size_t>::max();

void draw_sprite(SDL_Renderer* renderer, SDL_Texture* texture,
                 const SDL_FRect& destination, bool burning)
{
    Uint8 red = 255, green = 255, blue = 255;
    const bool tint = burning && SDL_GetTextureColorMod(texture, &red, &green, &blue);
    if (burning && !tint) {
        SDL_Log("Cannot read tree tint: %s", SDL_GetError());
    }
    // A steady warm cast identifies fuel even when the shared particle pool
    // is saturated. It is derived from fire state, so pause freezes the cue.
    if (tint && !SDL_SetTextureColorMod(texture, 255, 130, 110)) {
        SDL_Log("Cannot tint burning tree: %s", SDL_GetError());
    }
    if (!SDL_RenderTexture(renderer, texture, nullptr, &destination)) {
        SDL_Log("Cannot draw tree: %s", SDL_GetError());
    }
    // Sprites are shared by many trees; the next tree keeps its own appearance.
    if (tint && !SDL_SetTextureColorMod(texture, red, green, blue)) {
        SDL_Log("Cannot restore tree tint: %s", SDL_GetError());
    }
}
} // namespace

ActorRenderer::~ActorRenderer()
{
    shutdown();
}

bool ActorRenderer::initialize(TextureCache& textures)
{
    shutdown();
    for (std::size_t index = 0; index < sprites_.size(); ++index) {
        const std::string key = std::string("map/") + sprite_names[index];
        const auto* image = textures.load(key);
        if (!image) {
            SDL_Log("Cannot load required sprite %s: %s", key.c_str(), textures.error().c_str());
            shutdown(); return false;
        }
        sprites_[index].texture = image->texture; // Borrowed from the shared cache.
        masks_[index] = image->mask;
    }
    if (!settlers_.initialize(textures)) { shutdown(); return false; }
    return true;
}

ActorHandle ActorRenderer::pick(const TileMap& map, const std::vector<ActorView>& actors,
                          const MapView& view,
                                 glm::vec2 framebuffer_scale, glm::vec2 logical_mouse) const
{
    build_actor_scene(map, actors, {view.origin, view.radius, framebuffer_scale}, masks_, scene_);
    return pick_actor(scene_, masks_, logical_mouse);
}

void ActorRenderer::draw(SDL_Renderer* renderer, const TileMap& map, const std::vector<ActorView>& actors,
                          const MapView& view,
                        const ParticleSystem* particles, std::uint64_t tick, std::uint64_t generation) const
{
    glm::vec2 framebuffer_scale;
    SDL_GetRenderScale(renderer, &framebuffer_scale.x, &framebuffer_scale.y);
    const ActorSceneView scene_view{view.origin, view.radius, framebuffer_scale};
    build_actor_scene(map, actors, scene_view, masks_, scene_);
    if (view.radius <= 0.0f) {
        return;
    }
    settlers_.prepare(actors, scene_view, tick, generation);
    if (particles) {
        particle_renderer_.prepare(*particles, scene_view);
    }
    const auto& groups = particle_renderer_.groups();
    grouped_.assign(particles ? groups.size() : 0, false);
    draw_entries_.clear();
    draw_entries_.reserve(scene_.size() + grouped_.size());
    for (std::size_t index = 0; index < scene_.size(); ++index) {
        const ActorSceneEntry& tree = scene_[index];
        std::size_t group = no_entry;
        if (particles) {
            const auto found = std::lower_bound(groups.begin(), groups.end(), tree.id,
                [](const ParticleRenderer::Group& candidate, std::uint64_t id) {
                    return candidate.emitter_id < id;
                });
            if (found != groups.end() && found->emitter_id == tree.id) {
                group = static_cast<std::size_t>(found - groups.begin());
                grouped_[group] = true;
            }
        }
        draw_entries_.push_back({index, no_entry, group, tree.ground, tree.variant, tree.id});
    }
    for (std::size_t index = 0; index < grouped_.size(); ++index) {
        if (!grouped_[index]) {
            draw_entries_.push_back({no_entry, no_entry, index, groups[index].ground, 0, groups[index].emitter_id});
        }
    }
    for (std::size_t index = 0; index < settlers_.entries().size(); ++index) {
        const auto& settler = settlers_.entries()[index];
        draw_entries_.push_back({no_entry, index, no_entry, settler.layout.ground, 0, settler.id});
    }
    std::sort(draw_entries_.begin(), draw_entries_.end(), [](const DrawEntry& a, const DrawEntry& b) {
        if (a.ground.y != b.ground.y) {
            return a.ground.y < b.ground.y;
        }
        if (a.ground.x != b.ground.x) {
            return a.ground.x < b.ground.x;
        }
        if (a.variant != b.variant) {
            return a.variant < b.variant;
        }
        return a.id < b.id;
    });
    for (const DrawEntry& entry : draw_entries_) {
        if (entry.particle_group != no_entry) {
            particle_renderer_.draw(renderer, groups[entry.particle_group], ParticleKind::smoke, scene_view);
        }
        if (entry.settler != no_entry) settlers_.draw(renderer, settlers_.entries()[entry.settler]);
        if (entry.actor != no_entry) {
            const ActorSceneEntry& tree = scene_[entry.actor];
            const SDL_FRect destination{tree.minimum.x, tree.minimum.y, tree.size.x, tree.size.y};
            if (sprites_[tree.sprite_index].texture) {
                draw_sprite(renderer, sprites_[tree.sprite_index].texture, destination,
                             tree.burning);
            }
        }
        if (entry.particle_group != no_entry) {
            const ParticleRenderer::Group& group = groups[entry.particle_group];
            particle_renderer_.draw(renderer, group, ParticleKind::flame, scene_view);
            particle_renderer_.draw(renderer, group, ParticleKind::ember, scene_view);
        }
    }
}

void ActorRenderer::shutdown()
{
    settlers_.shutdown();
    for (Sprite& sprite : sprites_) {
        sprite = {};
    }
    for (ActorSpriteMask& mask : masks_) {
        mask = {};
    }
    scene_.clear();
    draw_entries_.clear();
    grouped_.clear();
}
