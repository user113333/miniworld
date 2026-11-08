#include "tree_renderer.h"
#include "hex_grid.h"

#include <algorithm>
#include <string>
#include <vector>

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
        const std::string path = std::string(base_path) + "res/map/tree" + std::to_string(index + 1) + ".png";
        SDL_Surface* image = SDL_LoadPNG(path.c_str());
        if (!image) {
            SDL_Log("Cannot load %s: %s", path.c_str(), SDL_GetError());
            shutdown();
            return false;
        }
        Sprite& sprite = sprites_[index];
        sprite.size = {image->w, image->h};
        sprite.texture = SDL_CreateTextureFromSurface(renderer, image);
        SDL_DestroySurface(image);
        if (!sprite.texture || !SDL_SetTextureScaleMode(sprite.texture, SDL_SCALEMODE_NEAREST)
            || !SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_BLEND)) {
            SDL_Log("Cannot create tree texture: %s", SDL_GetError());
            shutdown();
            return false;
        }
    }
    return true;
}

void TreeRenderer::draw(SDL_Renderer* renderer, const TileMap& map, const MapView& view) const
{
    if (view.radius <= 0.0f) {
        return;
    }
    std::vector<const Tree*> ordered;
    ordered.reserve(map.trees.size());
    for (const Tree& tree : map.trees) {
        ordered.push_back(&tree);
    }
    std::sort(ordered.begin(), ordered.end(), [](const Tree* a, const Tree* b) {
        return a->hex.y == b->hex.y ? a->hex.x < b->hex.x : a->hex.y < b->hex.y;
    });

    glm::vec2 framebuffer_scale;
    SDL_GetRenderScale(renderer, &framebuffer_scale.x, &framebuffer_scale.y);
    const float tile_width = glm::sqrt(3.0f) * view.radius;
    for (const Tree* tree : ordered) {
        if (tree->variant >= sprites_.size()) {
            continue;
        }
        const Sprite& sprite = sprites_[tree->variant];
        if (!sprite.texture) {
            continue;
        }
        // Scale source pixels by an integer in the physical framebuffer. The
        // bottom-center is the ground anchor, snapped to that same pixel grid.
        const float pixel_scale = glm::max(1.0f, glm::floor(tile_width * framebuffer_scale.x
                                                          * 0.8f / sprite.size.x));
        const glm::vec2 size = glm::vec2(sprite.size) * pixel_scale / framebuffer_scale;
        glm::vec2 ground = view.origin + hex_center(tree->hex) * view.radius;
        ground = glm::round(ground * framebuffer_scale) / framebuffer_scale;
        const SDL_FRect destination{ground.x - size.x * 0.5f, ground.y - size.y, size.x, size.y};
        SDL_RenderTexture(renderer, sprite.texture, nullptr, &destination);
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
}
