#pragma once

#include "actor.h"
#include <array>
#include <string>
#include <vector>

enum class ItemType { empty, seeds, logs };
struct InventorySlot { ItemType type = ItemType::empty; int quantity = 0; };
struct Inventory {
    std::array<InventorySlot, 8> slots{};
    int count(ItemType type) const;
    int add(ItemType type, int quantity); // Returns the quantity accepted; excess is discarded.
    bool consume(ItemType type);
};
struct SettlerDna {
    float speed = 1.0f;
    std::array<float, 4> preferences{1, 1, 1, 1}; // idle, wander, cut, plant
    float people_sensitivity = 1.0f;
    float tree_sensitivity = 1.0f;
};
enum class SettlerAction { idle, wander, cut, plant };
struct Settler {
    SettlerDna dna;
    Inventory inventory;
    int happiness = 500;
    SettlerAction action = SettlerAction::idle;
    std::string thought;
    std::uint64_t thought_ticks = 0;
    std::uint64_t next_thought = 0;
    std::uint64_t remaining_ticks = 0;
    std::uint64_t action_ticks = 0;
    std::uint64_t idle_ticks = 0;
    std::uint64_t active_ticks = 0;
    double mean_active_seconds = 8;
    std::uint64_t random_state = 0;
    ActorHandle target;
    glm::ivec2 destination{};
    std::vector<glm::ivec2> route;
    std::size_t waypoint = 0;
};

inline bool operator==(const InventorySlot& a, const InventorySlot& b)
{ return a.type == b.type && a.quantity == b.quantity; }
inline bool operator==(const Inventory& a, const Inventory& b) { return a.slots == b.slots; }

namespace flecs { struct world; struct entity; }
flecs::entity create_settler(flecs::world& ecs, ActorId id, ActorPosition position, const Settler& state);
const char* settler_action_name(SettlerAction action);
