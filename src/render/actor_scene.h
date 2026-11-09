#pragma once

#include "world/tile_map.h"
#include "actors/actor_view.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

struct ActorSpriteMask {
    glm::ivec2 size{};
    std::vector<std::uint8_t> alpha;
};

struct ActorSceneView {
    glm::vec2 origin{};
    float radius = 0.0f;
    glm::vec2 framebuffer_scale{1.0f};
};

struct ActorSceneEntry {
    ActorHandle handle;
    std::size_t sprite_index;
    std::uint64_t id;
    unsigned variant;
    glm::vec2 ground;
    glm::vec2 minimum;
    glm::vec2 size;
    bool burning;
    bool selectable;
};

std::size_t actor_sprite_index(const ActorView& actor);
float actor_source_pixel_scale(const ActorSceneView& view, int source_width = 8);
glm::vec2 snapped_actor_ground(glm::vec2 world_ground, const ActorSceneView& view);
void build_actor_scene(const TileMap& map, const std::vector<ActorView>& actors,
                       const ActorSceneView& view,
                      const std::array<ActorSpriteMask, 5>& sprites,
                      std::vector<ActorSceneEntry>& scene);
ActorHandle pick_actor(const std::vector<ActorSceneEntry>& scene,
                        const std::array<ActorSpriteMask, 5>& sprites,
                        glm::vec2 logical_mouse);

bool actor_depth_less(const ActorSceneEntry& a, const ActorSceneEntry& b);
bool sprite_hit(const ActorSpriteMask& sprite, glm::vec2 minimum, glm::vec2 size,
                glm::vec2 mouse, bool flipped = false);
