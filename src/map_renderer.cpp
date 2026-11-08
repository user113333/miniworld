#include "map_renderer.h"
#include "hex_grid.h"

#include <array>
#include <vector>

void draw_map(SDL_Renderer* renderer, const TileMap& map, const SDL_FRect& area)
{
    if (area.w <= 0.0f || area.h <= 0.0f) {
        return;
    }
    // Regular pointy-top hexagons: width sqrt(3)*radius, height 2*radius.
    const glm::vec2 size{glm::sqrt(3.0f) * (map.width + 0.5f), 1.5f * (map.height - 1) + 2.0f};
    const float radius = glm::min(area.w / size.x, area.h / size.y);
    const glm::vec2 origin = glm::vec2{area.x, area.y}
                           + (glm::vec2{area.w, area.h} - size * radius) * 0.5f
                           + glm::vec2{glm::sqrt(3.0f) * 0.5f, 1.0f} * radius;
    const glm::vec2 scale = hex_lattice_scale(radius);
    constexpr std::array<SDL_FColor, 4> colors = {{
        {89 / 255.0f, 113 / 255.0f, 135 / 255.0f, 1},  // Water
        {197 / 255.0f, 173 / 255.0f, 103 / 255.0f, 1}, // Sand
        {168 / 255.0f, 144 / 255.0f, 91 / 255.0f, 1},  // Land
        {147 / 255.0f, 159 / 255.0f, 75 / 255.0f, 1},  // Grass
    }};
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    vertices.reserve(map.tiles.size() * 6);
    indices.reserve(map.tiles.size() * 12);
    for (int index = 0; index < static_cast<int>(map.tiles.size()); ++index) {
        const glm::ivec2 center = hex_lattice_center(map.hex_at(index));
        const int first = static_cast<int>(vertices.size());
        const SDL_FColor color = colors[static_cast<int>(map.tiles[index])];
        for (glm::ivec2 corner : hex_corners) {
            const glm::vec2 point = origin + glm::vec2(center + corner) * scale;
            vertices.push_back({{point.x, point.y}, color, {0, 0}});
        }
        for (int corner = 1; corner < 5; ++corner) {
            indices.insert(indices.end(), {first, first + corner, first + corner + 1});
        }
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}
