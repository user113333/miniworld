#include "world/island_generator.h"
#include "test_support.h"
#include "world/tree_placement.h"

#include <fstream>
#include <locale>
#include <set>

namespace {
void expect_token(std::istream& input, const char* expected)
{
    std::string token;
    test::expect(static_cast<bool>(input >> token) && token == expected, "Expected fixture token: " + std::string(expected));
}

char terrain_symbol(Terrain terrain)
{
    switch (terrain) {
    case Terrain::water: return '.';
    case Terrain::sand: return 's';
    case Terrain::land: return 'l';
    case Terrain::grass: return 'g';
    }
    throw std::runtime_error("Unknown terrain");
}

void check_fixture(const char* name)
{
    std::ifstream input(std::string(TEST_FIXTURE_DIR) + "/" + name);
    input.imbue(std::locale::classic());
    test::expect(input.is_open(), "Cannot open seed fixture");
    int width = 0;
    int height = 0;
    std::uint32_t seed = 0;
    float density = 0.0f;
    expect_token(input, "size");
    input >> width >> height;
    expect_token(input, "seed");
    input >> seed;
    expect_token(input, "density");
    test::expect(static_cast<bool>(input >> density) && density >= 0.0f && density <= 1.0f, "Invalid fixture density");
    test::MapState map = generate_island(width, height, seed);
    place_trees(map, density);
    expect_token(input, "terrain");
    for (int row = 0; row < height; ++row) {
        std::string expected;
        test::expect(static_cast<bool>(input >> expected) && expected.size() == static_cast<std::size_t>(width),
                     "Invalid terrain row " + std::to_string(row));
        for (int column = 0; column < width; ++column) {
            test::expect(terrain_symbol(map.tiles[row * width + column]) == expected[column],
                         "Terrain changed at column " + std::to_string(column) + ", row " + std::to_string(row));
        }
    }
    expect_token(input, "trees");
    std::size_t tree_count = 0;
    test::expect(static_cast<bool>(input >> tree_count) && tree_count == map.trees.size(), "Tree count changed");
    const auto actual = test::trees_by_cell(map);
    std::set<int> seen;
    for (std::size_t record = 0; record < tree_count; ++record) {
        test::TreeRecord expected{};
        test::expect(static_cast<bool>(input >> expected.hex.x >> expected.hex.y >> expected.variant
                                      >> expected.offset.x >> expected.offset.y), "Invalid tree record");
        test::expect(map.contains(expected.hex), "Fixture tree is outside map");
        const int index = map.index(expected.hex);
        test::expect(seen.insert(index).second, "Duplicate fixture tree");
        const auto found = actual.find(index);
        const std::string context = "tree (" + std::to_string(expected.hex.x) + ", " + std::to_string(expected.hex.y) + ")";
        test::expect(found != actual.end(), context + " moved or disappeared");
        test::expect(found->second.variant == expected.variant, context + " changed sprite variant");
        test::expect(test::near(found->second.offset.x, expected.offset.x, 0.000001f)
                     && test::near(found->second.offset.y, expected.offset.y, 0.000001f), context + " changed ground offset");
    }
    std::string trailing;
    test::expect(!(input >> trailing), "Unexpected data after fixture trees");
}
} // namespace

int main()
{
    return test::run("Seed fixtures", [] {
        for (const char* name : {"seed_0.txt", "seed_74129.txt", "seed_max.txt"}) {
            test::with_context(name, [&] { check_fixture(name); });
        }
    });
}
