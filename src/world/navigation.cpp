#include "navigation.h"
#include <algorithm>
#include <cmath>
#include <limits>

bool walkable(const TileMap& map, glm::ivec2 hex)
{
    if (!map.contains(hex)) return false;
    switch (map.tiles[map.index(hex)]) {
    case Terrain::sand: case Terrain::land: case Terrain::grass: case Terrain::burned: return true;
    default: return false; // Water and future terrain types (including mountains) block movement.
    }
}
glm::ivec2 world_hex(glm::vec2 p)
{
    const float q = p.x / glm::sqrt(3.0f) - p.y / 3.0f;
    const float r = p.y * (2.0f / 3.0f);
    glm::vec3 cube{q, r, -q-r};
    glm::vec3 rounded = glm::round(cube);
    const auto error = glm::abs(rounded - cube);
    if (error.x > error.y && error.x > error.z) rounded.x = -rounded.y - rounded.z;
    else if (error.y > error.z) rounded.y = -rounded.x - rounded.z;
    return {static_cast<int>(rounded.x), static_cast<int>(rounded.y)};
}
std::vector<glm::ivec2> find_route(const TileMap& map, glm::ivec2 start, glm::ivec2 goal, int radius)
{
    radius = std::clamp(radius, 0, 10);
    if (!walkable(map, start) || !walkable(map, goal) || hex_distance(start, goal) > radius) return {};
    struct Node { glm::ivec2 hex; int cost; int parent; bool closed = false; };
    std::vector<Node> nodes{{start, 0, -1}};
    for (;;) {
        int best = -1, score = std::numeric_limits<int>::max();
        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            const int candidate = nodes[i].cost + hex_distance(nodes[i].hex, goal);
            if (!nodes[i].closed && candidate < score) { best = i; score = candidate; }
        }
        if (best < 0) return {};
        const auto current = nodes[best];
        if (current.hex == goal) {
            std::vector<glm::ivec2> path;
            for (int i = best; i >= 0; i = nodes[i].parent) path.push_back(nodes[i].hex);
            std::reverse(path.begin(), path.end());
            return path;
        }
        nodes[best].closed = true;
        for (auto delta : hex_neighbors) {
            const auto next = current.hex + delta;
            if (hex_distance(start, next) > radius || !walkable(map, next)) continue;
            auto found = std::find_if(nodes.begin(), nodes.end(), [&](const Node& n) { return n.hex == next; });
            if (found == nodes.end()) nodes.push_back({next, current.cost + 1, best});
            else if (current.cost + 1 < found->cost) { found->cost = current.cost + 1; found->parent = best; found->closed = false; }
        }
    }
}
