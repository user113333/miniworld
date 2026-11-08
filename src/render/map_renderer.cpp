#include "render/map_renderer.h"
#include "world/hex_grid.h"

#include <array>
#include <vector>

SDL_FColor terrain_color(Terrain terrain, bool weathered)
{
    if (terrain == Terrain::burned && weathered) {
        return {138 / 255.0f, 121 / 255.0f, 102 / 255.0f, 1}; // Weathered earth
    }
    constexpr std::array<SDL_FColor, 5> colors = {{
        {89 / 255.0f, 113 / 255.0f, 135 / 255.0f, 1},  // Water
        {197 / 255.0f, 173 / 255.0f, 103 / 255.0f, 1}, // Sand
        {168 / 255.0f, 144 / 255.0f, 91 / 255.0f, 1},  // Land
        {147 / 255.0f, 159 / 255.0f, 75 / 255.0f, 1},  // Grass
        {91 / 255.0f, 78 / 255.0f, 69 / 255.0f, 1},   // Burned
    }};
    return colors[static_cast<int>(terrain)];
}

MapView fit_map(const TileMap& map, const SDL_FRect& area)
{
    // Regular pointy-top hexagons: width sqrt(3)*radius, height 2*radius.
    const glm::vec2 storage_center = hex_center(map.storage_origin());
    const glm::vec2 minimum = -glm::vec2{glm::sqrt(3.0f) * 0.5f, 1.0f} - storage_center;
    const glm::vec2 maximum = glm::vec2{glm::sqrt(3.0f) * map.width,
                                       1.5f * (map.height - 1) + 1.0f} - storage_center;
    const glm::vec2 half_extent = glm::max(glm::abs(minimum), glm::abs(maximum));
    const float radius = glm::max(0.0f, glm::min(area.w / (2 * half_extent.x),
                                               area.h / (2 * half_extent.y)));
    const glm::vec2 origin = glm::vec2{area.x, area.y}
                           + glm::vec2{area.w, area.h} * 0.5f;
    return {origin, radius};
}

void draw_map(SDL_Renderer* renderer, const TileMap& map, const MapView& view)
{
    if (view.radius <= 0.0f) {
        return;
    }
    const glm::vec2 scale = hex_lattice_scale(view.radius);
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    vertices.reserve(map.tiles.size() * 6);
    indices.reserve(map.tiles.size() * 12);
    for (int index = 0; index < static_cast<int>(map.tiles.size()); ++index) {
        const glm::ivec2 center = hex_lattice_center(map.hex_at(index));
        const int first = static_cast<int>(vertices.size());
        const bool weathered = static_cast<std::size_t>(index) < map.recovery.size()
                               && map.recovery[index].weathered();
        const SDL_FColor color = terrain_color(map.tiles[index], weathered);
        for (glm::ivec2 corner : hex_corners) {
            const glm::vec2 point = view.origin + glm::vec2(center + corner) * scale;
            vertices.push_back({{point.x, point.y}, color, {0, 0}});
        }
        for (int corner = 1; corner < 5; ++corner) {
            indices.insert(indices.end(), {first, first + corner, first + corner + 1});
        }
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}
