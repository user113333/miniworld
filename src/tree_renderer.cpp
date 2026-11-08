#include "tree_renderer.h"

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

TreeRenderer::~TreeRenderer()
{
    shutdown();
}

bool TreeRenderer::initialize(SDL_Renderer* renderer)
{
    shutdown();
    const char* base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_Log("Cannot locate tree sprites: %s", SDL_GetError());
        return false;
    }
    for (std::size_t index = 0; index < sprites_.size(); ++index) {
        const std::string path = std::string(base_path) + "res/map/" + sprite_names[index];
        SDL_Surface* image = SDL_LoadPNG(path.c_str());
        if (!image) {
            SDL_Log("Cannot load %s: %s", path.c_str(), SDL_GetError());
            shutdown();
            return false;
        }
        TreeSpriteMask& mask = masks_[index];
        mask.size = {image->w, image->h};
        mask.alpha.resize(static_cast<std::size_t>(image->w * image->h));
        bool readable = true;
        for (int y = 0; y < image->h && readable; ++y) {
            for (int x = 0; x < image->w; ++x) {
                Uint8 red, green, blue, alpha;
                if (!SDL_ReadSurfacePixel(image, x, y, &red, &green, &blue, &alpha)) {
                    readable = false;
                    break;
                }
                mask.alpha[y * image->w + x] = alpha;
            }
        }
        Sprite& sprite = sprites_[index];
        sprite.texture = readable ? SDL_CreateTextureFromSurface(renderer, image) : nullptr;
        SDL_DestroySurface(image);
        if (!sprite.texture || !SDL_SetTextureScaleMode(sprite.texture, SDL_SCALEMODE_NEAREST)
            || !SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_BLEND)) {
            SDL_Log("Cannot create tree texture/mask: %s", SDL_GetError());
            shutdown();
            return false;
        }
    }
    return true;
}

std::uint64_t TreeRenderer::pick(const TileMap& map, const MapView& view,
                                 glm::vec2 framebuffer_scale, glm::vec2 logical_mouse) const
{
    build_tree_scene(map, {view.origin, view.radius, framebuffer_scale}, masks_, scene_);
    return pick_tree(scene_, masks_, logical_mouse);
}

void TreeRenderer::draw(SDL_Renderer* renderer, const TileMap& map, const MapView& view,
                        const ParticleSystem* particles) const
{
    glm::vec2 framebuffer_scale;
    SDL_GetRenderScale(renderer, &framebuffer_scale.x, &framebuffer_scale.y);
    const TreeSceneView scene_view{view.origin, view.radius, framebuffer_scale};
    build_tree_scene(map, scene_view, masks_, scene_);
    if (view.radius <= 0.0f) {
        return;
    }
    if (particles) {
        particle_renderer_.prepare(*particles, scene_view);
    }
    const auto& groups = particle_renderer_.groups();
    grouped_.assign(particles ? groups.size() : 0, false);
    draw_entries_.clear();
    draw_entries_.reserve(scene_.size() + grouped_.size());
    for (std::size_t index = 0; index < scene_.size(); ++index) {
        const TreeSceneEntry& tree = scene_[index];
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
        draw_entries_.push_back({index, group, tree.ground, tree.variant, tree.id});
    }
    for (std::size_t index = 0; index < grouped_.size(); ++index) {
        if (!grouped_[index]) {
            draw_entries_.push_back({no_entry, index, groups[index].ground, 0, groups[index].emitter_id});
        }
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
        if (entry.tree != no_entry) {
            const TreeSceneEntry& tree = scene_[entry.tree];
            const SDL_FRect destination{tree.minimum.x, tree.minimum.y, tree.size.x, tree.size.y};
            if (sprites_[tree.sprite_index].texture) {
                draw_sprite(renderer, sprites_[tree.sprite_index].texture, destination,
                             map.trees[tree.tree_index].burning());
            }
        }
        if (entry.particle_group != no_entry) {
            const ParticleRenderer::Group& group = groups[entry.particle_group];
            particle_renderer_.draw(renderer, group, ParticleKind::flame, scene_view);
            particle_renderer_.draw(renderer, group, ParticleKind::ember, scene_view);
        }
    }
}

void TreeRenderer::shutdown()
{
    for (Sprite& sprite : sprites_) {
        if (sprite.texture) {
            SDL_DestroyTexture(sprite.texture);
        }
        sprite = {};
    }
    for (TreeSpriteMask& mask : masks_) {
        mask = {};
    }
    scene_.clear();
    draw_entries_.clear();
    grouped_.clear();
}
