#include "world/hex_grid.h"
#include "world/island_generator.h"
#include "test_support.h"
#include "world/tree_placement.h"

#include <algorithm>
#include <limits>
#include <set>

namespace {
bool eligible(Terrain terrain)
{
    return terrain == Terrain::grass || terrain == Terrain::land;
}

void check_densities(test::MapState map)
{
    const auto terrain = map.tiles;
    const auto eligible_count = std::count_if(terrain.begin(), terrain.end(), eligible);
    std::map<int, test::TreeRecord> previous;
    for (float density : {-0.5f, 0.0f, 0.2f, 0.5f, 1.0f, 1.5f}) {
        test::with_context("density " + std::to_string(density), [&] {
            place_trees(map, density);
            test::expect(map.tiles == terrain, "Trees changed the underlying terrain");
            const auto expected_count = static_cast<std::size_t>(glm::round(
                static_cast<float>(eligible_count) * glm::clamp(density, 0.0f, 1.0f)));
            test::expect(map.trees.size() == expected_count, "Density does not match occupied fraction");
            const auto current = test::trees_by_cell(map);
            for (const auto& [index, tree] : current) {
                test::expect(eligible(map.tiles[index]), "Tree placed on water or sand");
                test::expect(tree.variant < 3, "Unknown sprite variant");
                test::expect(std::isfinite(tree.offset.x) && std::isfinite(tree.offset.y), "Invalid ground offset");
                test::expect(glm::length(tree.offset) < glm::sqrt(3.0f) * 0.5f,
                             "Tree root is outside its owning hex's inscribed circle");
            }
            for (const auto& [index, tree] : previous) {
                const auto found = current.find(index);
                test::expect(found != current.end(), "Increasing density removed an existing tree");
                test::expect_same_tree(found->second, tree);
            }
            place_trees(map, density);
            const auto repeated = test::trees_by_cell(map);
            test::expect(repeated.size() == current.size(), "Repeated placement changed tree count");
            for (const auto& [index, tree] : current) {
                const auto found = repeated.find(index);
                test::expect(found != repeated.end(), "Repeated placement moved a tree");
                test::expect_same_tree(found->second, tree);
            }
            previous = current;
        });
    }
}

void check_forest_groups()
{
    int grouped = 0;
    int total = 0;
    std::set<unsigned int> variants;
    for (std::uint32_t seed = 0; seed < 50; ++seed) {
        test::MapState map = generate_island(48, 32, seed);
        place_trees(map, 0.2f);
        const auto occupied = test::trees_by_cell(map);
        for (const test::TreeRecord& tree : map.trees) {
            int neighbors = 0;
            for (glm::ivec2 direction : hex_neighbors) {
                const glm::ivec2 neighbor = tree.hex + direction;
                if (map.contains(neighbor)) {
                    neighbors += static_cast<int>(occupied.count(map.index(neighbor)));
                }
            }
            grouped += neighbors >= 2;
            ++total;
            variants.insert(tree.variant);
        }
    }
    test::expect(total > 0, "Default density produced no forest");
    // Broad guard against uniform scattering; fixtures protect the exact layouts.
    test::expect(grouped > total * 0.75f, "Trees are scattered instead of forming forest groups");
    test::expect(variants.size() == 3, "Reference seeds do not exercise all three sprites");
}

void check_sparse_terrain()
{
    test::MapState map{12, 12, 42, std::vector<Terrain>(144, Terrain::water), {{{0, 0}, 0, {}}}};
    place_trees(map, 1.0f);
    test::expect(map.trees.empty(), "Old trees survived replacement on an empty ocean");
    std::fill(map.tiles.begin(), map.tiles.end(), Terrain::sand);
    place_trees(map, 1.0f);
    test::expect(map.trees.empty(), "Trees appeared on an all-sand map");
    map.tiles[map.index({0, 0})] = Terrain::grass;
    map.tiles[map.index({-1, 0})] = Terrain::land;
    check_densities(map);
}
} // namespace

int main()
{
    return test::run("Tree placement", [] {
        for (std::uint32_t seed : {0u, 1u, 74129u, std::numeric_limits<std::uint32_t>::max()}) {
            for (glm::ivec2 size : {glm::ivec2{12, 12}, {13, 71}, {48, 32}, {96, 72}}) {
                const std::string context = "seed " + std::to_string(seed) + ", "
                    + std::to_string(size.x) + "x" + std::to_string(size.y);
                test::with_context(context, [&] { check_densities(generate_island(size.x, size.y, seed)); });
            }
        }
        check_sparse_terrain();
        check_forest_groups();
    });
}
