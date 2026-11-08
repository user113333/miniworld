#include "tree_scene.h"
#include "hex_grid.h"

#include <algorithm>
#include <cmath>

namespace {
bool valid_view(const TreeSceneView& view)
{
    return std::isfinite(view.radius) && view.radius > 0.0f &&
           std::isfinite(view.origin.x) && std::isfinite(view.origin.y) &&
           std::isfinite(view.framebuffer_scale.x) && view.framebuffer_scale.x > 0.0f &&
           std::isfinite(view.framebuffer_scale.y) && view.framebuffer_scale.y > 0.0f;
}
} // namespace

std::size_t tree_sprite_index(const Tree& tree)
{
    switch (tree.stage) {
    case TreeStage::growing:
        return 3;
    case TreeStage::mature:
        return tree.variant < 3 ? tree.variant : 5;
    case TreeStage::dead:
        return 4;
    }
    return 5;
}

float tree_source_pixel_scale(const TreeSceneView& view, int source_width)
{
    if (!valid_view(view) || source_width <= 0) {
        return 0.0f;
    }
    const float tile_width = glm::sqrt(3.0f) * view.radius;
    return glm::max(1.0f, glm::floor(tile_width * view.framebuffer_scale.x * 1.05f / source_width));
}

glm::vec2 snapped_tree_ground(glm::vec2 world_ground, const TreeSceneView& view)
{
    const glm::vec2 ground = view.origin + world_ground * view.radius;
    return glm::round(ground * view.framebuffer_scale) / view.framebuffer_scale;
}

void build_tree_scene(const TileMap& map, const TreeSceneView& view,
                      const std::array<TreeSpriteMask, 5>& sprites,
                      std::vector<TreeSceneEntry>& scene)
{
    scene.clear();
    if (!valid_view(view)) {
        return;
    }
    scene.reserve(map.trees.size());
    for (std::size_t index = 0; index < map.trees.size(); ++index) {
        const Tree& tree = map.trees[index];
        const std::size_t sprite_index = tree_sprite_index(tree);
        if (sprite_index >= sprites.size() || !map.contains(tree.hex)) {
            continue;
        }
        const TreeSpriteMask& sprite = sprites[sprite_index];
        if (sprite.size.x <= 0 || sprite.size.y <= 0) {
            continue;
        }
        const glm::vec2 ground = snapped_tree_ground(hex_center(tree.hex) + tree.offset, view);
        const float pixel_scale = tree_source_pixel_scale(view, sprite.size.x);
        const glm::vec2 size = glm::vec2(sprite.size) * pixel_scale / view.framebuffer_scale;
        scene.push_back({index, sprite_index, tree.id, tree.variant, ground,
                         ground - glm::vec2{size.x * 0.5f, size.y}, size});
    }
    std::sort(scene.begin(), scene.end(), [](const TreeSceneEntry& a, const TreeSceneEntry& b) {
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
}

std::uint64_t pick_tree(const std::vector<TreeSceneEntry>& scene,
                        const std::array<TreeSpriteMask, 5>& sprites,
                        glm::vec2 logical_mouse)
{
    if (!std::isfinite(logical_mouse.x) || !std::isfinite(logical_mouse.y)) {
        return 0;
    }
    for (auto position = scene.rbegin(); position != scene.rend(); ++position) {
        const TreeSceneEntry& entry = *position;
        const glm::vec2 relative = (logical_mouse - entry.minimum) / entry.size;
        if (relative.x < 0.0f || relative.y < 0.0f || relative.x >= 1.0f || relative.y >= 1.0f) {
            continue;
        }
        const TreeSpriteMask& sprite = sprites[entry.sprite_index];
        const glm::ivec2 source = glm::ivec2(glm::floor(relative * glm::vec2(sprite.size)));
        const std::size_t pixel = static_cast<std::size_t>(source.y * sprite.size.x + source.x);
        if (pixel < sprite.alpha.size() && sprite.alpha[pixel] != 0) {
            return entry.id;
        }
    }
    return 0;
}
