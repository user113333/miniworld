#include "tree_placement.h"
#include "hex_grid.h"

#include <algorithm>

namespace {
std::uint32_t tree_hash(glm::ivec2 hex, std::uint32_t seed)
{
    std::uint32_t hash = seed ^ 0x51ed270bu
        ^ (static_cast<std::uint32_t>(hex.x) * 0x9e3779b9u)
        ^ (static_cast<std::uint32_t>(hex.y) * 0x85ebca6bu);
    hash = (hash ^ (hash >> 16)) * 0x7feb352du;
    hash = (hash ^ (hash >> 15)) * 0x846ca68bu;
    return hash ^ (hash >> 16);
}

float forest_noise(glm::vec2 position, std::uint32_t seed)
{
    const glm::ivec2 cell(glm::floor(position));
    const glm::vec2 fraction = glm::fract(position);
    const glm::vec2 blend = fraction * fraction * (3.0f - 2.0f * fraction);
    const auto sample = [seed](glm::ivec2 point) {
        return static_cast<float>(tree_hash(point, seed ^ 0x68bc21ebu) & 0xffffffu) / 16777216.0f;
    };
    return glm::mix(
        glm::mix(sample(cell), sample(cell + glm::ivec2{1, 0}), blend.x),
        glm::mix(sample(cell + glm::ivec2{0, 1}), sample(cell + glm::ivec2{1, 1}), blend.x), blend.y);
}
} // namespace

float forest_suitability(glm::ivec2 hex, std::uint32_t seed)
{
    const float suitability = 1.0f - forest_noise(hex_center(hex) / 9.0f, seed);
    return suitability * suitability;
}

void place_trees(TileMap& map, float density)
{
    map.trees.clear();
    density = glm::clamp(density, 0.0f, 1.0f);
    struct Candidate {
        glm::ivec2 hex;
        std::uint32_t hash;
        float score;
    };
    std::vector<Candidate> candidates;
    for (int index = 0; index < static_cast<int>(map.tiles.size()); ++index) {
        const Terrain terrain = map.tiles[index];
        if (terrain != Terrain::grass && terrain != Terrain::land) {
            continue;
        }
        const glm::ivec2 hex = map.hex_at(index);
        const std::uint32_t hash = tree_hash(hex, map.seed);
        const float grain = static_cast<float>(hash & 0xffffffu) / 16777216.0f;
        const float score = forest_noise(hex_center(hex) / 9.0f, map.seed) * 0.94f + grain * 0.06f;
        candidates.push_back({hex, hash, score});
    }
    // Fill coherent low-noise patches first. Density controls the fraction of
    // eligible tiles occupied, while increasing it keeps existing trees intact.
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.score != b.score) {
            return a.score < b.score;
        }
        return a.hex.y == b.hex.y ? a.hex.x < b.hex.x : a.hex.y < b.hex.y;
    });
    const auto count = static_cast<std::size_t>(glm::round(candidates.size() * density));
    map.trees.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const Candidate& candidate = candidates[index];
        const std::uint32_t position_hash = tree_hash(candidate.hex, map.seed ^ 0xa511e9b3u);
        const glm::vec2 fraction{static_cast<float>(position_hash & 0xffffu) / 65535.0f,
                                 static_cast<float>(position_hash >> 16) / 65535.0f};
        // The whole offset square lies within the hex's inscribed circle.
        const glm::vec2 offset = (fraction - 0.5f) * 0.8f;
        map.trees.push_back({candidate.hex, (candidate.hash >> 24) % 3, offset});
    }
}
