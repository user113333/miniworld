#pragma once

#include "tile_map.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

struct TreeSpriteMask {
    glm::ivec2 size{};
    std::vector<std::uint8_t> alpha;
};

struct TreeSceneView {
    glm::vec2 origin{};
    float radius = 0.0f;
    glm::vec2 framebuffer_scale{1.0f};
};

struct TreeSceneEntry {
    std::size_t tree_index;
    std::size_t sprite_index;
    std::uint64_t id;
    unsigned variant;
    glm::vec2 ground;
    glm::vec2 minimum;
    glm::vec2 size;
};

std::size_t tree_sprite_index(const Tree& tree);
float tree_source_pixel_scale(const TreeSceneView& view, int source_width = 8);
glm::vec2 snapped_tree_ground(glm::vec2 world_ground, const TreeSceneView& view);
void build_tree_scene(const TileMap& map, const TreeSceneView& view,
                      const std::array<TreeSpriteMask, 5>& sprites,
                      std::vector<TreeSceneEntry>& scene);
std::uint64_t pick_tree(const std::vector<TreeSceneEntry>& scene,
                        const std::array<TreeSpriteMask, 5>& sprites,
                        glm::vec2 logical_mouse);
