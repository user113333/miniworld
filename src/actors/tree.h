#pragma once

#include "actor.h"

#include <cstdint>

struct Tree {
    std::uint64_t decay_ticks = 0;
    std::uint64_t dead_ticks = 0;
    std::uint64_t ignition_tick = 0;
    std::uint64_t burnout_tick = 0;
    bool charred = false;

    bool burning() const { return burnout_tick != 0; }
};

struct TreeSpawn {
    glm::ivec2 hex{};
    unsigned variant = 0;
    glm::vec2 offset{};
};

namespace flecs { struct world; struct entity; }
flecs::entity create_tree(flecs::world& ecs, ActorId id,
                          const TreeSpawn& spawn, const Tree& state);
