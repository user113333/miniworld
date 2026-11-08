#include "island_generator.h"
#include "hex_grid.h"

#include <algorithm>
#include <stdexcept>

namespace {
float lattice_noise(glm::ivec2 point, std::uint32_t seed)
{
    std::uint32_t hash = seed ^ (static_cast<std::uint32_t>(point.x) * 0x9e3779b9u)
                              ^ (static_cast<std::uint32_t>(point.y) * 0x85ebca6bu);
    hash = (hash ^ (hash >> 16)) * 0x7feb352du;
    hash = (hash ^ (hash >> 15)) * 0x846ca68bu;
    hash ^= hash >> 16;
    return static_cast<float>(hash & 0xffffffu) / 8388607.5f - 1.0f;
}

float noise(glm::vec2 point, std::uint32_t seed)
{
    const glm::ivec2 cell(glm::floor(point));
    const glm::vec2 fraction = glm::fract(point);
    const glm::vec2 blend = fraction * fraction * (3.0f - 2.0f * fraction);
    return glm::mix(
        glm::mix(lattice_noise(cell, seed), lattice_noise(cell + glm::ivec2{1, 0}, seed), blend.x),
        glm::mix(lattice_noise(cell + glm::ivec2{0, 1}, seed),
                 lattice_noise(cell + glm::ivec2{1, 1}, seed), blend.x), blend.y);
}

void keep_main_island(TileMap& map)
{
    std::vector<bool> visited(map.tiles.size(), false);
    // Keep the component containing the island's origin, never an offshore fragment.
    const int start = map.index({0, 0});
    std::vector<int> connected{start};
    visited[start] = true;
    for (std::size_t next = 0; next < connected.size(); ++next) {
        const glm::ivec2 hex = map.hex_at(connected[next]);
        for (glm::ivec2 direction : hex_neighbors) {
            const glm::ivec2 neighbor = hex + direction;
            if (!map.contains(neighbor)) {
                continue;
            }
            const int index = map.index(neighbor);
            if (!visited[index] && map.tiles[index] != Terrain::water) {
                visited[index] = true;
                connected.push_back(index);
            }
        }
    }
    std::fill(map.tiles.begin(), map.tiles.end(), Terrain::water);
    for (int index : connected) {
        map.tiles[index] = Terrain::grass;
    }
}
} // namespace

TileMap generate_island(int width, int height, std::uint32_t seed)
{
    if (width < 12 || width > 96 || height < 12 || height > 72) {
        throw std::invalid_argument("Island dimensions must be 12..96 by 12..72");
    }
    TileMap map{width, height, seed, std::vector<Terrain>(width * height, Terrain::water)};
    const glm::vec2 half_extent{glm::sqrt(3.0f) * (width - 0.5f) * 0.5f,
                                1.5f * (height - 1) * 0.5f};
    for (int index = 0; index < width * height; ++index) {
        const int column = index % width;
        const int row = index / width;
        if (column == 0 || row == 0 || column == width - 1 || row == height - 1) {
            continue;
        }
        const glm::vec2 position = hex_center(map.hex_at(index)) / half_extent;
        const float elevation = 0.68f - glm::length(position)
                              + 0.24f * noise(position * 2.8f, seed)
                              + 0.08f * noise(position * 6.0f, seed ^ 0xa341316cu);
        if (elevation > 0.0f) {
            map.tiles[index] = Terrain::grass;
        }
    }
    keep_main_island(map);

    // Classify from a fixed land mask so coast detection does not depend on order.
    const auto land_mask = map.tiles;
    for (int index = 0; index < width * height; ++index) {
        if (land_mask[index] == Terrain::water) {
            continue;
        }
        const glm::ivec2 hex = map.hex_at(index);
        const bool coastal = std::any_of(hex_neighbors.begin(), hex_neighbors.end(), [&](glm::ivec2 direction) {
            const glm::ivec2 neighbor = hex + direction;
            return !map.contains(neighbor) || land_mask[map.index(neighbor)] == Terrain::water;
        });
        const glm::vec2 position = hex_center(hex) / half_extent;
        if (coastal) {
            map.tiles[index] = Terrain::sand;
        } else {
            map.tiles[index] = noise(position * 3.0f, seed ^ 0xc8013ea4u) < -0.20f
                ? Terrain::land : Terrain::grass;
        }
    }
    return map;
}
