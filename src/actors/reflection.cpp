#include "reflection.h"
#include "tree.h"
#include "settler.h"
#include <flecs.h>
#include <cstddef>

void register_actor_reflection(flecs::world& world)
{
    // Explicit offsets preserve GLM's actual layout, including aligned builds.
    world.component<glm::ivec2>("HexCoordinates")
        .member<int>("x", 1, offsetof(glm::ivec2, x))
        .member<int>("y", 1, offsetof(glm::ivec2, y));
    world.component<glm::vec2>("GroundOffset")
        .member<float>("x", 1, offsetof(glm::vec2, x))
        .member<float>("y", 1, offsetof(glm::vec2, y));
    world.component<ActorKind>(); // Flecs registers the C++ enum constants.
    world.component<ActorIdentity>().member(&ActorIdentity::value, "value");
    world.component<ActorPosition>()
        .member(&ActorPosition::hex, "hex").member(&ActorPosition::offset, "offset");
    world.component<ActorAppearance>()
        .member(&ActorAppearance::kind, "kind").member(&ActorAppearance::variant, "variant");
    world.component<Tree>()
        .member(&Tree::decay_ticks, "decay_ticks").member(&Tree::dead_ticks, "dead_ticks")
        .member(&Tree::ignition_tick, "ignition_tick").member(&Tree::burnout_tick, "burnout_tick")
        .member(&Tree::charred, "charred").member(&Tree::growth_ticks, "growth_ticks");
    world.component<SettlerAction>();
    world.component<SettlerDna>()
        .member(&SettlerDna::speed, "speed")
        .member(&SettlerDna::people_sensitivity, "people_sensitivity")
        .member(&SettlerDna::tree_sensitivity, "tree_sensitivity");
    world.component<Settler>()
        .member(&Settler::dna, "dna").member(&Settler::happiness, "happiness")
        .member(&Settler::action, "action").member(&Settler::thought_ticks, "thought_ticks")
        .member(&Settler::next_thought, "next_thought").member(&Settler::remaining_ticks, "remaining_ticks");
    world.component<Selectable>();
}
