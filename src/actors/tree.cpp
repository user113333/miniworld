#include "tree.h"
#include <flecs.h>

flecs::entity create_tree(flecs::world& ecs, ActorId id,
                          const TreeSpawn& spawn, const Tree& state)
{
    return ecs.entity()
        .set<ActorIdentity>({id})
        .set<ActorPosition>({spawn.hex, spawn.offset})
        .set<ActorAppearance>({ActorKind::tree, spawn.variant})
        .set<Tree>(state)
        .add<Selectable>();
}
