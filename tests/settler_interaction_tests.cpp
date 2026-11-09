#include "simulation/simulation.h"
#include "test_support.h"
#include <limits>
#include <algorithm>

namespace {
EntityEditResult apply(World& world, EntityEdit edit)
{
    const auto request = world.submit_edit(std::move(edit)); world.flush_edits();
    return *world.take_edit_result(request);
}
void check_registry_and_inventory()
{
    World world;
    world.reset({12, 12, 42, std::vector<Terrain>(144, Terrain::grass), {}}, {});
    const auto h = world.spawn_settler({});
    const auto entity = world.registry().entity(h.entity);
    test::expect(entity.has<Settler>() && entity.has<ActorIdentity>() && entity.has<ActorPosition>(), "settler is not stored in Flecs");
    const auto entities = world.entities();
    const auto found = std::find_if(entities.begin(), entities.end(), [&](const auto& summary) { return summary.handle == h; });
    test::expect(found != entities.end() && found->kind == "Settler" && found->path.find("Settler ") != std::string::npos,
                 "settler missing from entity explorer data or has no recognizable name");
    const auto inspection = *world.inspect_entity(h);
    int fields = 0;
    for (const auto& c : inspection.components) if (c.id == world.registry().id<Settler>()) {
        for (const auto& f : c.properties) {
            test::expect(!f.editable, "simulation-owned settler fields are directly editable");
            ++fields;
        }
    }
    test::expect(fields >= 28, "settler component data is opaque");
    const auto before = entity.get<Settler>();
    Inventory full;
    for (int i = 0; i < 8; ++i) full.slots[i] = {i % 2 ? ItemType::logs : ItemType::seeds, 10};
    EntityEdit edit{h}; edit.inventory = InventoryEdit{before.inventory, full};
    const auto request = world.submit_edit(edit);
    test::expect(entity.get<Settler>().inventory == before.inventory, "inventory edited before command boundary");
    world.flush_edits();
    test::expect(world.take_edit_result(request)->applied && entity.get<Settler>().inventory == full, "eight-slot edit failed");
    test::expect(entity.get<Settler>().random_state == before.random_state && entity.get<Settler>().happiness == 500, "inventory edit overwrote other state");
    edit.inventory->expected = full;
    edit.inventory->value.slots[7].quantity = 11;
    edit.properties = {{world.registry().id<ActorPosition>(), "hex.x", std::int64_t{0}, std::int64_t{1}}};
    test::expect(!apply(world, edit).applied && entity.get<Settler>().inventory == full && entity.get<ActorPosition>().hex == glm::ivec2{}, "invalid group partially applied");
    edit.properties.clear();
    for (int invalid : {-1, 11}) {
        edit.inventory->value = full; edit.inventory->value.slots[0].quantity = invalid;
        test::expect(!apply(world, edit).applied, "out-of-range quantity accepted");
    }
    edit.inventory->value = full; edit.inventory->value.slots[0].type = static_cast<ItemType>(9);
    test::expect(!apply(world, edit).applied, "unknown item accepted");
    edit.inventory->value.slots[0] = {ItemType::empty, 3};
    test::expect(!apply(world, edit).applied, "units assigned to empty type");
    edit.inventory->value = full; edit.inventory->value.slots[0] = {ItemType::seeds, 0};
    test::expect(apply(world, edit).applied && entity.get<Settler>().inventory.slots[0].type == ItemType::empty, "zero quantity not normalized");
    edit.inventory->expected = entity.get<Settler>().inventory;
    edit.inventory->value = full;
    const auto first = world.submit_edit(edit), second = world.submit_edit(edit);
    world.flush_edits();
    test::expect(world.take_edit_result(first)->applied && !world.take_edit_result(second)->applied, "FIFO conflict check failed");
    // A harvest/plant may change inventory while a draft is open.
    auto live = entity.get<Settler>(); live.inventory.consume(ItemType::seeds); entity.set<Settler>(live);
    test::expect(!apply(world, edit).applied, "changed live inventory was overwritten");
    world.destroy(h);
    const auto reused = world.spawn_settler({});
    test::expect(!apply(world, edit).applied && reused != h, "stale actor handle edited reused entity");
    EntityEdit wrong{world.spawn_tree({{1, 0}, 0, {}}, {})}; wrong.inventory = InventoryEdit{};
    test::expect(!apply(world, wrong).applied, "tree accepted inventory edit");
}
void check_spawn()
{
    Simulation simulation;
    simulation.island_settings.seed = 74129; simulation.regenerate(); simulation.settings.speed = 0;
    auto& world = simulation.world();
    int settlers = 0;
    world.registry().each([&](const Settler&) { ++settlers; });
    test::expect(settlers == 3, "initial settlers are not three Flecs entities");
    glm::ivec2 land{}, water{};
    for (int i = 0; i < static_cast<int>(simulation.map().tiles.size()); ++i) {
        if (simulation.map().tiles[i] == Terrain::grass) land = simulation.map().hex_at(i);
        if (simulation.map().tiles[i] == Terrain::water) water = simulation.map().hex_at(i);
    }
    const auto initial = world.actors().size();
    const auto request = world.submit_spawn({{land, {.2f, .1f}}, world.generation()});
    test::expect(world.actors().size() == initial, "spawn mutated registry immediately");
    simulation.advance_simulation(0);
    const auto result = *world.take_edit_result(request);
    const auto spawned = world.inspect(result.spawned);
    test::expect(result.applied && spawned && spawned->settler && spawned->position.offset == glm::vec2(.2f, .1f), "queued spawn lost click position");
    test::expect(spawned->settler->happiness == 500 && spawned->settler->inventory == Inventory{} &&
                 spawned->settler->dna.speed >= .5f && spawned->settler->dna.speed <= 1.5f, "new settler has invalid state");
    test::expect(simulation.clock().ticks() == 0, "paused spawn advanced clock");
    for (const SpawnSettler command : {
            SpawnSettler{{water, {}}, world.generation()},
            SpawnSettler{{{std::numeric_limits<int>::max(), 0}, {}}, world.generation()},
            SpawnSettler{{land, {2, 0}}, world.generation()},
            SpawnSettler{{land, {std::numeric_limits<float>::quiet_NaN(), 0}}, world.generation()},
            SpawnSettler{{land, {}}, world.generation() - 1}}) {
        const auto invalid = world.submit_spawn(command); simulation.advance_simulation(0);
        test::expect(!world.take_edit_result(invalid)->applied, "invalid spawn accepted");
    }
    const int cell = world.map().index(land);
    const auto terrain = world.map().tiles[cell]; world.map().tiles[cell] = static_cast<Terrain>(99);
    auto invalid = world.submit_spawn({{land, {}}, world.generation()}); simulation.advance_simulation(0);
    test::expect(!world.take_edit_result(invalid)->applied, "future mountain terrain accepted");
    world.map().tiles[cell] = terrain;
    const auto occupant = world.actor_at(cell);
    const auto same = world.submit_spawn({{land, {}}, world.generation()}); simulation.advance_simulation(0);
    test::expect(world.take_edit_result(same)->applied && world.actor_at(cell) == occupant, "spawn adds actor collision or overwrites tree occupancy");
    const auto old_generation = world.generation();
    const auto pending = world.submit_spawn({{land, {}}, old_generation});
    EntityEdit old{result.spawned}; old.inventory = InventoryEdit{};
    simulation.regenerate(); simulation.advance_simulation(0);
    test::expect(!world.take_edit_result(pending) && !world.alive(result.spawned), "regeneration kept queued spawn or actor");
    test::expect(!apply(world, old).applied, "regeneration permits stale inventory edit");
    const auto stale = world.submit_spawn({{land, {}}, old_generation}); simulation.advance_simulation(0);
    test::expect(!world.take_edit_result(stale)->applied, "old-world click spawns after reset");
}
}
int main() { return test::run("Settler interaction", [] { check_registry_and_inventory(); check_spawn(); }); }
