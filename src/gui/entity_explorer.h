#pragma once
#include "simulation/entity_inspection.h"

class Simulation;
class EntityExplorer {
public:
    void open();
    void select(EntityHandle handle);
    EntityHandle selected() const { return selected_; }
    void protect_selection(bool protect) { protected_selection_ = protect; }
    bool is_open() const { return open_; }
    void draw(Simulation& simulation);
private:
    void switch_selection(EntityHandle handle);
    void reload(const EntityInspection& live);
    void draw_details(Simulation& simulation, const EntityInspection& live);
    bool open_ = false, focus_ = false, all_ = false;
    bool dirty_ = false, ignite_ = false, protected_selection_ = false;
    double burn_seconds_ = 16.0;
    int kind_ = 0, sort_ = 0;
    char search_[192]{}, component_filter_[128]{};
    EntityHandle selected_, scope_, submitted_target_;
    std::optional<EntityHandle> requested_selection_;
    std::uint64_t generation_ = 0, pending_ = 0;
    double next_refresh_ = 0;
    std::vector<EntitySummary> entities_;
    std::optional<EntityInspection> captured_;
    std::vector<EntityProperty> staged_;
    std::string message_;
};
