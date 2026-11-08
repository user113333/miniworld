#include "world/hex_grid.h"
#include "world/island_generator.h"
#include "test_support.h"

#include <limits>
#include <set>

namespace {
void check_island(const test::MapState& map)
{
    const int count = map.width * map.height;
    test::expect(map.tiles.size() == static_cast<std::size_t>(count), "Wrong terrain count");
    test::expect(map.trees.empty(), "Terrain generation unexpectedly places trees");
    const Terrain center = map.tiles[map.index({0, 0})];
    test::expect(center == Terrain::grass || center == Terrain::land, "Origin must be inland");
    int island_tiles = 0;
    for (int index = 0; index < count; ++index) {
        const Terrain terrain = map.tiles[index];
        test::expect(terrain >= Terrain::water && terrain <= Terrain::grass, "Unknown terrain value");
        const int column = index % map.width;
        const int row = index / map.width;
        if (column == 0 || column == map.width - 1 || row == 0 || row == map.height - 1) {
            test::expect(terrain == Terrain::water, "Island touches the map boundary");
        }
        if (terrain == Terrain::water) {
            continue;
        }
        ++island_tiles;
        bool coastal = false;
        for (glm::ivec2 direction : hex_neighbors) {
            const glm::ivec2 neighbor = map.hex_at(index) + direction;
            coastal |= !map.contains(neighbor) || map.tiles[map.index(neighbor)] == Terrain::water;
        }
        test::expect((terrain == Terrain::sand) == coastal, "Coastal sand classification changed");
    }

    // Independently count the land reachable from the origin.
    std::set<int> visited{map.index({0, 0})};
    std::vector<int> pending{map.index({0, 0})};
    while (!pending.empty()) {
        const glm::ivec2 hex = map.hex_at(pending.back());
        pending.pop_back();
        for (glm::ivec2 direction : hex_neighbors) {
            const glm::ivec2 neighbor = hex + direction;
            if (!map.contains(neighbor)) {
                continue;
            }
            const int index = map.index(neighbor);
            if (map.tiles[index] != Terrain::water && visited.insert(index).second) {
                pending.push_back(index);
            }
        }
    }
    test::expect(static_cast<int>(visited.size()) == island_tiles, "Disconnected land fragment");
}

void check_invalid_dimensions()
{
    for (glm::ivec2 size : {glm::ivec2{-1, 32}, {0, 32}, {11, 32}, {97, 32},
                           {48, -1}, {48, 0}, {48, 11}, {48, 73},
                           {std::numeric_limits<int>::max(), 32}}) {
        bool rejected = false;
        try {
            generate_island(size.x, size.y, 0);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        test::expect(rejected, "Invalid dimensions accepted: " + std::to_string(size.x) + "x" + std::to_string(size.y));
    }
}
} // namespace

int main()
{
    return test::run("Island generator", [] {
        check_invalid_dimensions();
        for (std::uint32_t seed : {0u, 1u, 42u, 74129u, std::numeric_limits<std::uint32_t>::max()}) {
            for (glm::ivec2 size : {glm::ivec2{12, 12}, {13, 13}, {48, 32}, {49, 33}, {96, 72}, {12, 72}, {96, 12}}) {
                const std::string context = "seed " + std::to_string(seed) + ", "
                    + std::to_string(size.x) + "x" + std::to_string(size.y);
                test::with_context(context, [&] {
                    const test::MapState map = generate_island(size.x, size.y, seed);
                    test::expect(map.width == size.x && map.height == size.y && map.seed == seed, "Wrong map metadata");
                    check_island(map);
                    test::expect(generate_island(size.x, size.y, seed).tiles == map.tiles, "Seed is not reproducible");
                });
            }
        }
        const test::MapState standard = generate_island(48, 32, 74129);
        test::expect(std::set<Terrain>(standard.tiles.begin(), standard.tiles.end()).size() == 4,
                     "Reference island must contain all four terrain types");
        std::set<std::vector<Terrain>> layouts;
        for (std::uint32_t seed = 0; seed < 10; ++seed) {
            layouts.insert(generate_island(48, 32, seed).tiles);
        }
        test::expect(layouts.size() == 10, "Different reference seeds produced identical islands");
    });
}
