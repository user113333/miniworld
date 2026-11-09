#include "settler.h"
#include <algorithm>

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
