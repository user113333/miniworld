#include "render/actor_scene.h"
#include "world/hex_grid.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace {
bool valid_view(const ActorSceneView& view)
{
    return std::isfinite(view.radius) && view.radius > 0.0f &&
           std::isfinite(view.origin.x) && std::isfinite(view.origin.y) &&
           std::isfinite(view.framebuffer_scale.x) && view.framebuffer_scale.x > 0.0f &&
           std::isfinite(view.framebuffer_scale.y) && view.framebuffer_scale.y > 0.0f;
}
} // namespace

std::size_t actor_sprite_index(const ActorView& actor)
{
    if (actor.appearance.kind != ActorKind::tree || !actor.tree) return 5;
    const Tree& tree = *actor.tree;
    if (tree.charred) return 4;
    return actor.appearance.variant < 3 ? actor.appearance.variant : 5;
}

float actor_source_pixel_scale(const ActorSceneView& view, int source_width)
{
    if (!valid_view(view) || source_width <= 0) {
        return 0.0f;
    }
    const float tile_width = glm::sqrt(3.0f) * view.radius;
    return tile_width * view.framebuffer_scale.x * 1.05f / source_width;
}

glm::vec2 snapped_actor_ground(glm::vec2 world_ground, const ActorSceneView& view)
{
    const glm::vec2 ground = view.origin + world_ground * view.radius;
    return glm::round(ground * view.framebuffer_scale) / view.framebuffer_scale;
}

void build_actor_scene(const TileMap& map, const std::vector<ActorView>& actors,
                       const ActorSceneView& view,
                      const std::array<ActorSpriteMask, 5>& sprites,
                      std::vector<ActorSceneEntry>& scene)
{
    scene.clear();
    if (!valid_view(view)) {
        return;
    }
    scene.reserve(actors.size());
    for (const ActorView& actor : actors) {
        const std::size_t sprite_index = actor_sprite_index(actor);
        if (sprite_index >= sprites.size() || !map.contains(actor.position.hex)) {
            continue;
        }
        const ActorSpriteMask& sprite = sprites[sprite_index];
        if (sprite.size.x <= 0 || sprite.size.y <= 0) {
            continue;
        }
        const glm::vec2 ground = snapped_actor_ground(hex_center(actor.position.hex) + actor.position.offset, view);
        const float pixel_scale = actor_source_pixel_scale(view, sprite.size.x);
        const glm::vec2 size = glm::vec2(sprite.size) * pixel_scale / view.framebuffer_scale;
        scene.push_back({actor.handle, sprite_index, actor.id, actor.appearance.variant, ground,
                         ground - glm::vec2{size.x * 0.5f, size.y}, size,
                         actor.tree && actor.tree->burning(), actor.selectable});
    }
    std::sort(scene.begin(), scene.end(), actor_depth_less);
}

ActorHandle pick_actor(const std::vector<ActorSceneEntry>& scene,
                        const std::array<ActorSpriteMask, 5>& sprites,
                        glm::vec2 logical_mouse)
{
    if (!std::isfinite(logical_mouse.x) || !std::isfinite(logical_mouse.y)) {
        return {};
    }
    for (auto position = scene.rbegin(); position != scene.rend(); ++position) {
        const ActorSceneEntry& entry = *position;
        if (sprite_hit(sprites[entry.sprite_index], entry.minimum, entry.size, logical_mouse))
            return entry.selectable ? entry.handle : ActorHandle{};
    }
    return {};
}

bool actor_depth_less(const ActorSceneEntry& a, const ActorSceneEntry& b)
{
    return std::tie(a.ground.y, a.ground.x, a.variant, a.id) < std::tie(b.ground.y, b.ground.x, b.variant, b.id);
}
bool sprite_hit(const ActorSpriteMask& sprite, glm::vec2 minimum, glm::vec2 size, glm::vec2 mouse, bool flipped)
{
    if (!std::isfinite(mouse.x) || !std::isfinite(mouse.y) || !std::isfinite(size.x) || !std::isfinite(size.y) ||
        size.x <= 0 || size.y <= 0 || sprite.size.x <= 0 || sprite.size.y <= 0) return false;
    const glm::vec2 relative = (mouse - minimum) / size;
    if (!std::isfinite(relative.x) || !std::isfinite(relative.y)) return false;
    if (relative.x < 0 || relative.y < 0 || relative.x >= 1 || relative.y >= 1) return false;
    auto source = glm::ivec2(glm::floor(relative * glm::vec2(sprite.size)));
    if (flipped) source.x = sprite.size.x - 1 - source.x;
    const auto pixel = static_cast<std::size_t>(source.y * sprite.size.x + source.x);
    return pixel < sprite.alpha.size() && sprite.alpha[pixel] != 0;
}
