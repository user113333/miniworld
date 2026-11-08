#include "tree_renderer.h"
#include "hex_grid.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
constexpr std::array<const char*, 5> sprite_names{
    "tree1.png", "tree2.png", "tree3.png", "tree_growing.png", "tree_dead.png"
};

std::size_t sprite_index(const Tree& tree)
{
    switch (tree.stage) {
    case TreeStage::growing:
        return 3;
    case TreeStage::mature:
        return tree.variant < 3 ? tree.variant : sprite_names.size();
    case TreeStage::dead:
        return 4;
    }
    return sprite_names.size();
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
    glm::vec2 framebuffer_scale;
    SDL_GetRenderScale(renderer, &framebuffer_scale.x, &framebuffer_scale.y);
    struct PositionedTree {
        const Tree* tree;
        glm::vec2 ground;
    };
    std::vector<PositionedTree> ordered;
    ordered.reserve(map.trees.size());
    for (const Tree& tree : map.trees) {
        glm::vec2 ground = view.origin + (hex_center(tree.hex) + tree.offset) * view.radius;
        ground = glm::round(ground * framebuffer_scale) / framebuffer_scale;
        ordered.push_back({&tree, ground});
    }
    std::sort(ordered.begin(), ordered.end(), [](const PositionedTree& a, const PositionedTree& b) {
        if (a.ground.y != b.ground.y) {
            return a.ground.y < b.ground.y;
        }
        if (a.ground.x != b.ground.x) {
            return a.ground.x < b.ground.x;
        }
        return a.tree->variant < b.tree->variant;
    });

    const float tile_width = glm::sqrt(3.0f) * view.radius;
    for (const PositionedTree& positioned : ordered) {
        const Tree* tree = positioned.tree;
        const std::size_t index = sprite_index(*tree);
        if (index >= sprites_.size()) {
            continue;
        }
        const Sprite& sprite = sprites_[index];
        if (!sprite.texture) {
            continue;
        }
        // Scale source pixels by an integer in the physical framebuffer. The
        // bottom-center is the ground anchor, snapped to that same pixel grid.
        // Canopies extend beyond their tiles so neighboring trees can overlap.
        const float pixel_scale = glm::max(1.0f, glm::floor(tile_width * framebuffer_scale.x
                                                          * 1.05f / sprite.size.x));
        const glm::vec2 size = glm::vec2(sprite.size) * pixel_scale / framebuffer_scale;
        const glm::vec2 ground = positioned.ground;
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
