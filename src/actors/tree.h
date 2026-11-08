#pragma once

#include <glm/glm.hpp>

#include <cstdint>

enum class TreeStage { growing, mature, dead };

struct Tree {
    glm::ivec2 hex;
    unsigned int variant;
    glm::vec2 offset{}; // Ground offset in hex-radius units, inside the owning tile.
    std::uint64_t id = 0;
    TreeStage stage = TreeStage::mature;
    std::uint64_t age_ticks = 0;
    std::uint64_t growth_ticks = 0;
    std::uint64_t lifespan_ticks = 0;
    std::uint64_t decay_ticks = 0;
    std::uint64_t dead_ticks = 0;
    std::uint64_t ignition_tick = 0;
    std::uint64_t burnout_tick = 0;
    bool charred = false;

    bool burning() const { return burnout_tick != 0; }
};
