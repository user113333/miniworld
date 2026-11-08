#include "world/hex_grid.h"
#include "test_support.h"

#include <algorithm>
#include <set>

namespace {
void check_storage(glm::ivec2 size)
{
    test::MapState map{size.x, size.y, 0, std::vector<Terrain>(size.x * size.y, Terrain::water), {}};
    const int middle = (size.y / 2) * size.x + size.x / 2;
    test::expect(map.hex_at(middle) == glm::ivec2(0, 0), "Middle tile is not world origin");
    test::expect(map.index({0, 0}) == middle, "World origin has wrong storage index");
    test::expect(map.contains({-1, 0}) && map.contains({0, -1}), "Negative world coordinates rejected");
    for (int index = 0; index < size.x * size.y; ++index) {
        const glm::ivec2 hex = map.hex_at(index);
        test::expect(map.contains(hex), "Stored tile is outside map bounds");
        test::expect(map.index(hex) == index, "Coordinate/index round trip failed");
    }
    for (int row = 0; row < size.y; ++row) {
        test::expect(!map.contains(map.hex_at(row * size.x) + glm::ivec2{-1, 0}), "Left edge wraps");
        test::expect(!map.contains(map.hex_at((row + 1) * size.x - 1) + glm::ivec2{1, 0}), "Right edge wraps");
    }
    for (int column = 0; column < size.x; ++column) {
        test::expect(!map.contains(map.hex_at(column) + glm::ivec2{0, -1}), "Top edge wraps");
        test::expect(!map.contains(map.hex_at((size.y - 1) * size.x + column) + glm::ivec2{0, 1}),
                     "Bottom edge wraps");
    }
}

void check_geometry()
{
    test::expect(test::near(hex_center({1, 0}).x, glm::sqrt(3.0f)), "Wrong horizontal spacing");
    test::expect(test::near(hex_center({0, 1}).x, glm::sqrt(3.0f) * 0.5f), "Wrong row stagger");
    test::expect(test::near(hex_center({0, 1}).y, 1.5f), "Wrong vertical spacing");
    const glm::ivec2 origin{-3, 2};
    std::set<std::pair<int, int>> neighbors;
    for (glm::ivec2 direction : hex_neighbors) {
        neighbors.emplace(direction.x, direction.y);
        test::expect(std::find(hex_neighbors.begin(), hex_neighbors.end(), -direction) != hex_neighbors.end(),
                     "Neighbor relation is not reciprocal");
        test::expect(test::near(glm::length(hex_center(direction)), glm::sqrt(3.0f)),
                     "Neighbor centers are not equidistant");
        int shared_corners = 0;
        for (glm::ivec2 corner : hex_corners) {
            for (glm::ivec2 neighbor_corner : hex_corners) {
                shared_corners += hex_lattice_center(origin) + corner
                    == hex_lattice_center(origin + direction) + neighbor_corner;
            }
        }
        test::expect(shared_corners == 2, "Adjacent hexes must share exactly one edge");
    }
    test::expect(neighbors.size() == 6, "Six distinct neighbors required");
    for (std::size_t index = 0; index < hex_corners.size(); ++index) {
        const glm::vec2 corner = glm::vec2(hex_corners[index]) * hex_lattice_scale(1.0f);
        const glm::vec2 next = glm::vec2(hex_corners[(index + 1) % hex_corners.size()]) * hex_lattice_scale(1.0f);
        test::expect(test::near(glm::length(corner), 1.0f), "Hex is skewed: unequal corner radii");
        test::expect(test::near(glm::length(next - corner), 1.0f), "Hex is skewed: unequal edge lengths");
    }
}
} // namespace

int main()
{
    return test::run("Hex grid", [] {
        check_geometry();
        for (glm::ivec2 size : {glm::ivec2{12, 12}, {13, 13}, {12, 71}, {95, 12}, {48, 32}, {96, 72}}) {
            test::with_context(std::to_string(size.x) + "x" + std::to_string(size.y), [&] { check_storage(size); });
        }
        const test::MapState small{4, 4, 0, std::vector<Terrain>(16, Terrain::water), {}};
        test::expect(small.hex_at(0) == glm::ivec2(-1, -2), "Known upper-left coordinate changed");
        test::expect(small.hex_at(4) == glm::ivec2(-1, -1), "Odd-row coordinate conversion changed");
        test::expect(small.hex_at(15) == glm::ivec2(1, 1), "Known lower-right coordinate changed");
    });
}
