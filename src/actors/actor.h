#pragma once

#include <glm/glm.hpp>
#include <cstdint>

using ActorId = std::uint64_t;
enum class ActorKind { tree, settler };

// The full Flecs ID guards entity reuse; generation also guards world resets.
struct ActorHandle {
    std::uint64_t entity = 0;
    std::uint64_t generation = 0;
    explicit operator bool() const { return entity != 0; }
};
inline bool operator==(ActorHandle a, ActorHandle b)
{
    return a.entity == b.entity && a.generation == b.generation;
}
inline bool operator!=(ActorHandle a, ActorHandle b) { return !(a == b); }

struct ActorIdentity { ActorId value = 0; };
struct ActorPosition {
    glm::ivec2 hex{};
    glm::vec2 offset{}; // Ground offset in hex-radius units, inside the owning tile.
};
struct ActorAppearance {
    ActorKind kind = ActorKind::tree;
    unsigned variant = 0;
};
struct Selectable {};
