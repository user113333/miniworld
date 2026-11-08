#include "effects/fire_effects.h"
#include "world/hex_grid.h"

void collect_fire_emitters(const std::vector<ActorView>& actors, std::vector<FireEmitter>& emitters)
{
    emitters.clear();
    for (const ActorView& actor : actors) {
        if (!actor.tree || !actor.tree->burning()) {
            continue;
        }
        const Tree& tree = *actor.tree;
        // All sprites share an eight-pixel canvas; only their occupied canopy
        // height differs. These dimensions stay in world units when resizing.
        float height = 1.5f;
        if (tree.stage == TreeStage::growing) {
            height = 0.7f;
        } else if (tree.stage == TreeStage::dead) {
            height = 1.0f;
        }
        emitters.push_back({actor.id, hex_center(actor.position.hex) + actor.position.offset, height,
                            tree.ignition_tick, tree.burnout_tick});
    }
}
