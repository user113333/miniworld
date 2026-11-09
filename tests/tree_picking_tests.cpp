#include "render/actor_scene.h"
#include "test_support.h"

#include <algorithm>
#include <limits>

namespace {

std::array<ActorSpriteMask, 5> opaque_sprites()
{
    std::array<ActorSpriteMask, 5> sprites;
    for (ActorSpriteMask& sprite : sprites) {
        sprite.size = {8, 8};
        sprite.alpha.assign(64, 255);
    }
    return sprites;
}

test::MapState empty_map()
{
    return {12, 12, 42, std::vector<Terrain>(144, Terrain::grass), {}};
}

void check_frontmost_opaque_pixel()
{
    test::MapState map = empty_map();
    test::TreeRecord front{{0, 0}, 1, {0.35f, 0.3f}};
    test::TreeRecord back{{1, 0}, 0, {-0.35f, -0.3f}};
    front.id = 10;
    back.id = 20;
    map.trees = {front, back};
    auto sprites = opaque_sprites();
    const ActorSceneView view{{100, 100}, 20, {1, 1}};
    std::vector<ActorSceneEntry> scene;
    build_actor_scene(map, view, sprites, scene);
    test::expect(scene.size() == 2 && scene.back().id == front.id,
                 "Ground depth did not put the lower tree in front");
    test::expect(test::pick_id(scene, sprites, {117, 84}) == front.id,
                 "Overlap did not choose the visible front tree");
    const auto source_pixel = [](const ActorSceneEntry& entry) {
        const glm::ivec2 source(glm::floor((glm::vec2{117, 84} - entry.minimum) / entry.size * 8.0f));
        return source.y * 8 + source.x;
    };
    const int front_pixel = source_pixel(scene.back());
    const int back_pixel = source_pixel(scene.front());
    sprites[1].alpha[front_pixel] = 0;
    test::expect(test::pick_id(scene, sprites, {117, 84}) == back.id,
                 "Transparent source pixel blocked the tree behind it");
    sprites[0].alpha[back_pixel] = 0;
    test::expect(test::pick_id(scene, sprites, {117, 84}) == 0,
                 "Transparent pixels returned a tree");
    std::reverse(map.trees.begin(), map.trees.end());
    build_actor_scene(map, view, sprites, scene);
    sprites[1].alpha[front_pixel] = 255;
    test::expect(test::pick_id(scene, sprites, {117, 84}) == front.id,
                 "Tree vector order changed picking depth");
}

void check_pixel_mapping_and_bounds()
{
    test::MapState map = empty_map();
    test::TreeRecord tree{{0, 0}, 0, {0.123f, 0.234f}};
    tree.id = 7;
    map.trees.push_back(tree);
    for (glm::vec2 scale : {glm::vec2{1.0f}, {1.25f, 1.25f}, {1.5f, 1.5f},
                            {2, 2}, {1.25f, 2.0f}}) {
        auto sprites = opaque_sprites();
        sprites[0].alpha.assign(64, 0);
        sprites[0].alpha[3 * 8 + 2] = 255;
        const ActorSceneView view{{105.3f, 99.7f}, 13.2f, scale};
        std::vector<ActorSceneEntry> scene;
        build_actor_scene(map, view, sprites, scene);
        test::expect(scene.size() == 1, "DPI layout lost the tree");
        const ActorSceneEntry& entry = scene.front();
        const glm::vec2 physical_ground = entry.ground * scale;
        test::expect(test::near(physical_ground.x, glm::round(physical_ground.x)) &&
                     test::near(physical_ground.y, glm::round(physical_ground.y)),
                     "Ground is not snapped to physical pixels");
        const glm::vec2 physical_size = entry.size * scale;
        test::expect(test::near(physical_size.x, physical_size.y) &&
                     test::near(physical_size.x, 8 * actor_source_pixel_scale(view)),
                     "Source pixel scaling is not physically square");
        const glm::vec2 hit = entry.minimum + entry.size * glm::vec2{2.5f / 8, 3.5f / 8};
        test::expect(test::pick_id(scene, sprites, hit) == tree.id,
                     "Logical mouse did not map to the opaque source pixel at this DPI");
        const glm::vec2 clear = entry.minimum + entry.size * glm::vec2{1.5f / 8, 3.5f / 8};
        test::expect(test::pick_id(scene, sprites, clear) == 0, "Transparent source pixel picked at this DPI");
        test::expect(test::pick_id(scene, sprites, entry.minimum - glm::vec2{1, 1}) == 0 &&
                     test::pick_id(scene, sprites, entry.minimum + entry.size) == 0,
                     "Outside sprite rectangle picked a tree");
        for (float radius : {5.0f, 13.21f, 20.0f, 160.0f}) {
            build_actor_scene(map, {view.origin, radius, scale}, sprites, scene);
            test::expect(test::near(scene.front().size.x / radius, glm::sqrt(3.0f) * 1.05f),
                         "Sprite size must scale proportionally with terrain at every zoom");
        }
    }
}

void check_stages_and_invalid_input()
{
    test::MapState map = empty_map();
    test::TreeRecord tree{{0, 0}, 2, {}};
    tree.id = 5;
    map.trees.push_back(tree);
    auto sprites = opaque_sprites();
    std::vector<ActorSceneEntry> scene;
    const ActorSceneView view{{100, 100}, 10, {1, 1}};
    for (const auto [charred, expected_sprite] : {
             std::pair{false, 2u}, {true, 4u}}) {
        map.trees.front().charred = charred;
        build_actor_scene(map, view, sprites, scene);
        test::expect(scene.size() == 1 && scene.front().sprite_index == expected_sprite,
                     "Picking does not use the rendered life-stage sprite");
    }
    map.trees.front().hex = {100, 100};
    build_actor_scene(map, view, sprites, scene);
    test::expect(scene.empty(), "Out-of-map tree was pickable");
    map.trees.front().hex = {0, 0};
    build_actor_scene(map, {view.origin, 0, view.framebuffer_scale}, sprites, scene);
    test::expect(scene.empty(), "Zero-sized view produced pickable sprites");
    build_actor_scene(map, {view.origin, view.radius, {0, 1}}, sprites, scene);
    test::expect(scene.empty(), "Invalid framebuffer scale produced pickable sprites");
    build_actor_scene(map, view, sprites, scene);
    test::expect(test::pick_id(scene, sprites, {std::numeric_limits<float>::quiet_NaN(), 10}) == 0,
                 "Non-finite mouse input picked a tree");
}

} // namespace

int main()
{
    return test::run("Tree sprite picking", [] {
        check_frontmost_opaque_pixel();
        check_pixel_mapping_and_bounds();
        check_stages_and_invalid_input();
    });
}
