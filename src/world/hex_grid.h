#pragma once

#include <glm/glm.hpp>

#include <array>

inline const std::array<glm::ivec2, 6> hex_neighbors = {
    glm::ivec2{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {-1, 1}, {0, 1}
};

// Integer corner keys ensure adjacent hexagons produce identical shared vertices.
inline const std::array<glm::ivec2, 6> hex_corners = {
    glm::ivec2{0, -2}, {1, -1}, {1, 1}, {0, 2}, {-1, 1}, {-1, -1}
};

inline glm::ivec2 hex_lattice_center(glm::ivec2 hex)
{
    return {2 * hex.x + hex.y, 3 * hex.y};
}

inline glm::vec2 hex_lattice_scale(float radius)
{
    return glm::vec2{glm::sqrt(3.0f), 1.0f} * (radius * 0.5f);
}

inline glm::vec2 hex_center(glm::ivec2 hex)
{
    return glm::vec2(hex_lattice_center(hex)) * hex_lattice_scale(1.0f);
}
