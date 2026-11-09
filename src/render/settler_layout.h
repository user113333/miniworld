#pragma once
#include "actor_scene.h"
#include "world/hex_grid.h"

struct SettlerLayout {
    glm::vec2 ground;
    glm::vec2 minimum;
    glm::vec2 size;
    glm::vec2 head;
};
// MiniLumberjack's 32px canvas has feet at (14,31), and its head near y=17.
inline SettlerLayout settler_layout(const ActorPosition& position, const ActorSceneView& view)
{
    const auto ground = snapped_actor_ground(hex_center(position.hex) + position.offset, view);
    const float scale = view.radius / 12.0f;
    return {ground, ground - glm::vec2{14, 31} * scale, glm::vec2{32, 32} * scale,
            ground - glm::vec2{0, 14} * scale};
}
