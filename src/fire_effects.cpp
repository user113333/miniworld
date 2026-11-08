#include "fire_effects.h"
#include "hex_grid.h"

void collect_fire_emitters(const TileMap& map, std::vector<FireEmitter>& emitters)
{
    emitters.clear();
    for (const Tree& tree : map.trees) {
        if (!tree.burning()) {
            continue;
        }
        // All sprites share an eight-pixel canvas; only their occupied canopy
        // height differs. These dimensions stay in world units when resizing.
        float height = 1.5f;
        if (tree.stage == TreeStage::growing) {
            height = 0.7f;
        } else if (tree.stage == TreeStage::dead) {
            height = 1.0f;
        }
        emitters.push_back({tree.id, hex_center(tree.hex) + tree.offset, height,
                            tree.ignition_tick, tree.burnout_tick});
    }
}
