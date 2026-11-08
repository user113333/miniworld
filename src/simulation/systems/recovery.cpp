#include "simulation/world.h"
#include "random.h"

#include <algorithm>

void World::advance_recovery()
{
    for (std::size_t cell = 0; cell < map_.recovery.size(); ++cell) {
        auto& recovery = map_.recovery[cell];
        if (recovery.duration_ticks == 0) continue;
        recovery.elapsed_ticks = std::min(recovery.duration_ticks,
            recovery.elapsed_ticks + simulation_detail::world_ticks);
        if (recovery.elapsed_ticks == recovery.duration_ticks) {
            map_.tiles[cell] = recovery.original_terrain;
            recovery = {};
        }
    }
}
