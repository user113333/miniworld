#pragma once
#include "actors/settler.h"
#include "entity_tool.h"
#include <vector>

class Simulation;
class EntityExplorer;
class SettlersWindow {
public:
    void open() { open_ = true; focus_ = true; }
    bool is_open() const { return open_; }
    void draw(Simulation& simulation, EntityExplorer& explorer, EntityTool& tool);
    void spawn(Simulation& simulation, ActorPosition position, std::uint64_t generation);
private:
    void reload(const Settler& settler);
    bool open_ = false, dirty_ = false, focus_ = false;
    ActorHandle selected_, submitted_target_;
    std::uint64_t generation_ = 0, pending_ = 0;
    std::vector<std::uint64_t> pending_spawns_;
    Inventory captured_, staged_;
    std::string message_;
};
