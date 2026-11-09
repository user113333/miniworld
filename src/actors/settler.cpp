#include "settler.h"
#include <algorithm>
#include <flecs.h>

int Inventory::count(ItemType type) const
{
    int total = 0;
    for (const auto& slot : slots) if (slot.type == type) total += slot.quantity;
    return total;
}
int Inventory::add(ItemType type, int quantity)
{
    if (type == ItemType::empty || quantity <= 0) return 0;
    const int requested = quantity;
    for (auto& slot : slots) {
        if (slot.type != type) continue;
        const int amount = std::min(quantity, 10 - slot.quantity);
        slot.quantity += amount;
        quantity -= amount;
    }
    for (auto& slot : slots) {
        if (slot.quantity != 0) continue;
        const int amount = std::min(quantity, 10);
        if (amount) slot = {type, amount};
        quantity -= amount;
    }
    return requested - quantity;
}
bool Inventory::consume(ItemType type)
{
    for (auto& slot : slots) {
        if (slot.type != type || slot.quantity == 0) continue;
        if (--slot.quantity == 0) slot.type = ItemType::empty;
        return true;
    }
    return false;
}

flecs::entity create_settler(flecs::world& ecs, ActorId id, ActorPosition position, const Settler& state)
{
    return ecs.entity(("Settler " + std::to_string(id)).c_str())
        .set<ActorIdentity>({id}).set<ActorPosition>(position)
        .set<ActorAppearance>({ActorKind::settler, 0}).set<Settler>(state).add<Selectable>();
}
const char* settler_action_name(SettlerAction action)
{
    switch (action) {
    case SettlerAction::idle: return "Idle";
    case SettlerAction::wander: return "Wander";
    case SettlerAction::cut: return "Cut tree";
    case SettlerAction::plant: return "Plant tree";
    }
    return "Unknown";
}
