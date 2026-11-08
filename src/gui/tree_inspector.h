#pragma once

#include <cstdint>

class IslandDemo;

class TreeInspector {
public:
    void select(std::uint64_t tree_id, std::uint64_t generation);
    void draw(const IslandDemo& island);

private:
    std::uint64_t tree_id_ = 0;
    std::uint64_t generation_ = 0;
    bool focus_requested_ = false;
};
