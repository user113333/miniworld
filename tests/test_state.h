#pragma once

#include "actors/actor_view.h"
#include "world/tile_map.h"
#include "world/tree_placement.h"
#include "render/actor_scene.h"
#include "effects/fire_effects.h"

namespace test {

// Editable test inputs and comparison values, never production actor storage.
struct TreeRecord : Tree {
    glm::ivec2 hex{};
    unsigned variant = 0;
    glm::vec2 offset{};
    ActorId id = 0;

    TreeRecord() = default;
    TreeRecord(glm::ivec2 hex, unsigned variant, glm::vec2 offset)
        : hex(hex), variant(variant), offset(offset) {}
};

struct MapState : TileMap {
    std::vector<TreeRecord> trees;

    MapState(int width, int height, std::uint32_t seed, std::vector<Terrain> tiles,
             std::vector<TreeRecord> trees = {}, std::vector<BurnRecovery> recovery = {})
        : TileMap{width, height, seed, std::move(tiles), std::move(recovery)}, trees(std::move(trees)) {}
    MapState(TileMap map) : TileMap(std::move(map)) {}
};

inline TreeRecord record(const ActorView& actor)
{
    TreeRecord result{actor.position.hex, actor.appearance.variant, actor.position.offset};
    result.id = actor.id;
    if (actor.tree) static_cast<Tree&>(result) = *actor.tree;
    return result;
}

inline std::vector<ActorView> actor_views(const MapState& map)
{
    std::vector<ActorView> result;
    for (std::size_t i = 0; i < map.trees.size(); ++i) {
        const auto& tree = map.trees[i];
        result.push_back({{tree.id ? tree.id : i + 1, 1}, tree.id,
            {tree.hex, tree.offset}, {ActorKind::tree, tree.variant}, true,
            static_cast<const Tree&>(tree)});
    }
    return result;
}

inline void place_trees(MapState& map, float density)
{
    map.trees.clear();
    for (const auto& spawn : ::place_trees(map, density))
        map.trees.emplace_back(spawn.hex, spawn.variant, spawn.offset);
}

inline void build_actor_scene(const MapState& map, const ActorSceneView& view,
    const std::array<ActorSpriteMask, 5>& sprites, std::vector<ActorSceneEntry>& scene)
{
    ::build_actor_scene(map, actor_views(map), view, sprites, scene);
}

inline ActorId pick_id(const std::vector<ActorSceneEntry>& scene,
    const std::array<ActorSpriteMask, 5>& sprites, glm::vec2 mouse)
{
    return ::pick_actor(scene, sprites, mouse).entity;
}

inline void collect_fire_emitters(const MapState& map, std::vector<FireEmitter>& emitters)
{
    ::collect_fire_emitters(actor_views(map), emitters);
}

} // namespace test
