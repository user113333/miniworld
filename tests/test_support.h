#pragma once

#include "test_state.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace test {

inline void expect(bool condition, const std::string& message)
{
    // Unlike assert(), these checks also run when NDEBUG is defined.
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline bool near(float actual, float expected, float tolerance = 0.00001f)
{
    return std::isfinite(actual) && glm::abs(actual - expected) <= tolerance;
}

template<typename Function>
void with_context(const std::string& context, Function&& function)
{
    try {
        function();
    } catch (const std::exception& error) {
        throw std::runtime_error(context + ": " + error.what());
    }
}

template<typename Function>
int run(const char* name, Function&& function)
{
    try {
        function();
        std::cout << name << ": passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << name << ": " << error.what() << '\n';
        return 1;
    }
}

inline std::map<int, TreeRecord> trees_by_cell(const MapState& map)
{
    std::map<int, TreeRecord> result;
    for (const TreeRecord& tree : map.trees) {
        expect(map.contains(tree.hex), "TreeRecord lies outside the map");
        expect(result.emplace(map.index(tree.hex), tree).second, "Two trees occupy the same tile");
    }
    return result;
}

inline void expect_same_tree(const TreeRecord& actual, const TreeRecord& expected)
{
    expect(actual.hex == expected.hex, "TreeRecord coordinate changed");
    expect(actual.variant == expected.variant, "TreeRecord variant changed");
    expect(actual.offset == expected.offset, "TreeRecord ground offset changed");
}

} // namespace test
