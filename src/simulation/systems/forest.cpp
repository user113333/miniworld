#include "simulation/world.h"
#include "random.h"

using namespace simulation_detail;

void World::advance_tree(flecs::entity entity, const ActorIdentity& id,
                         const ActorPosition& position, Tree& tree)
{
    if (tree.burning()) {
        if (tick_ >= tree.burnout_tick) burn_down(id, position, tree);
        return;
    }
    if (tree.charred) {
        tree.dead_ticks += world_ticks;
        if (tree.dead_ticks >= tree.decay_ticks) decayed_.push_back({entity.id(), generation_});
    }
}
