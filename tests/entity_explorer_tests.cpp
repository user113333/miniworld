#include "simulation/simulation.h"
#include "render/actor_scene.h"
#include "test_support.h"
#include <algorithm>
#include <limits>
#include <set>

namespace {
TileMap map() { return {12, 12, 42, std::vector<Terrain>(144, Terrain::grass), {}}; }
const EntityProperty& field(const EntityInspection& details, std::uint64_t component, const std::string& path)
{
    for (const auto& c : details.components)
        for (const auto& f : c.properties) if (f.component == component && f.path == path) return f;
    throw std::runtime_error("Missing reflected field " + path);
}
PropertyEdit change(const World& world, EntityHandle target, std::uint64_t component,
                    const std::string& path, PropertyValue value)
{
    const auto details = world.inspect_entity(target);
    return {component, path, field(*details, component, path).value, std::move(value)};
}
EntityEditResult apply(World& world, EntityEdit edit)
{
    const auto request = world.submit_edit(std::move(edit));
    test::expect(request != 0, "Edit queue rejected request");
    world.flush_edits();
    const auto result = world.take_edit_result(request);
    test::expect(result.has_value(), "Edit has no acknowledgement");
    return *result;
}
struct Unknown { int value; };
struct Extra {
    bool active;
    double amount;
    const char* text;
    glm::vec2 vector;
    ecs_entity_t target;
};

void check_inspection()
{
    World world; world.reset(map(), {{{0, 0}, 2, {}}});
    auto& ecs = world.registry();
    ecs.component<Extra>()
        .member(&Extra::active, "active").member(&Extra::amount, "amount")
        .member(flecs::String, "text", 1, offsetof(Extra, text))
        .member(&Extra::vector, "vector")
        .member(flecs::Entity, "target", 1, offsetof(Extra, target));
    const auto parent = ecs.entity("Group");
    const auto child = ecs.entity().child_of(parent).add(flecs::Prefab).disable()
        .set<Unknown>({123}).set<Extra>({true, 2.5, "hello", {0.1f, 0.2f}, parent.id()});
    const EntityHandle target{child.id(), world.generation()};
    test::expect(world.entity_alive(target) && !world.alive(target), "General handles still require actor identity");
    const auto before = world.actors();
    const auto first = world.entities();
    const auto second = world.entities();
    test::expect(first.size() == static_cast<std::size_t>(ecs_get_entities(ecs.c_ptr()).alive_count), "Enumeration misses live entities");
    std::set<std::uint64_t> ids;
    bool system = false, component = false, module = false;
    for (std::size_t i = 0; i < first.size(); ++i) {
        test::expect(first[i].handle == second[i].handle && ids.insert(first[i].handle.entity).second,
                     "Enumeration is unstable or duplicated");
        if (i) test::expect(first[i-1].handle.entity < first[i].handle.entity, "List is not deterministically sorted");
        system |= first[i].kind == "System"; component |= first[i].kind == "Component"; module |= first[i].kind == "Module";
        test::expect(world.inspect_entity(first[i].handle).has_value(), "A live builtin cannot be inspected");
    }
    test::expect(system && component && module, "List misses registry categories");
    const auto details = *world.inspect_entity(target);
    test::expect(details.entity.disabled && details.entity.prefab && details.entity.parent.entity == parent.id(),
                 "Prefab/disabled state or hierarchy lost");
    test::expect(details.entity.path.find(std::to_string(child.id())) != std::string::npos, "Unnamed entity has no useful label");
    test::expect(std::get<bool>(field(details, ecs.id<Extra>(), "active").value) &&
                 std::get<std::string>(field(details, ecs.id<Extra>(), "text").value) == "hello" &&
                 std::get<double>(field(details, ecs.id<Extra>(), "amount").value) == 2.5,
                 "Generic scalar/string inspection failed");
    test::expect(field(details, ecs.id<Extra>(), "target").reference.entity == parent.id() &&
                 std::get<double>(field(details, ecs.id<Extra>(), "vector.x").value) > 0,
                 "Generic references or nested GLM reflection failed");
    bool unsupported = false, pair = false;
    for (const auto& c : details.components) {
        unsupported |= c.id == ecs.id<Unknown>() && c.detail.find("Unsupported") != std::string::npos;
        pair |= ECS_IS_PAIR(c.id) && c.references.size() == 2;
    }
    test::expect(unsupported && pair && world.actors().size() == before.size(), "Inspection hides unsupported data or mutates actors");
    const auto actor = world.actors().front().handle;
    const auto actor_details = *world.inspect_entity(actor);
    test::expect(!field(actor_details, ecs.id<ActorIdentity>(), "value").editable &&
                 !field(actor_details, ecs.id<Tree>(), "burnout_tick").editable &&
                 !field(actor_details, ecs.id<ActorAppearance>(), "kind").choices.empty(),
                 "Missing read-only policies or enum choices");
}

void check_edits_and_rendering()
{
    World world; world.reset(map(), {{{0, 0}, 0, {}}, {{2, 0}, 1, {}}});
    auto& ecs = world.registry();
    const auto target = world.actor_at(world.map().index({0, 0}));
    int notifications = 0;
    ecs.observer<const ActorPosition>().event(flecs::OnSet).each([&](const ActorPosition&) { ++notifications; });
    EntityEdit edit{target};
    edit.properties = {
        change(world, target, ecs.id<ActorPosition>(), "hex.x", std::int64_t{1}),
        change(world, target, ecs.id<ActorPosition>(), "offset.y", 0.2),
        change(world, target, ecs.id<ActorAppearance>(), "variant", std::uint64_t{2}),
        change(world, target, ecs.id<Selectable>(), "selectable", false)};
    test::expect(apply(world, edit).applied && notifications == 1, "Valid group or change notifications failed");
    test::expect(!world.actor_at(world.map().index({0, 0})) && world.actor_at(world.map().index({1, 0})) == target &&
                 !world.inspect(target)->selectable && world.inspect(target)->appearance.variant == 2,
                 "Accepted edits left stale occupancy or views");
    std::array<ActorSpriteMask, 5> sprites;
    for (auto& sprite : sprites) { sprite.size = {8, 8}; sprite.alpha.assign(64, 255); }
    std::vector<ActorSceneEntry> scene;
    build_actor_scene(world.map(), world.actors(), {{100, 100}, 20, {1, 1}}, sprites, scene);
    const auto entry = *std::find_if(scene.begin(), scene.end(), [&](const auto& e) { return e.handle == target; });
    test::expect(entry.sprite_index == 2 && !pick_actor(scene, sprites, entry.minimum + entry.size * 0.5f),
                 "Scene or picking ignored edited position/selection/appearance");
    EntityEdit select{target}; select.properties.push_back(change(world, target, ecs.id<Selectable>(), "selectable", true));
    test::expect(apply(world, select).applied, "Selection tag could not be restored");
    build_actor_scene(world.map(), world.actors(), {{100, 100}, 20, {1, 1}}, sprites, scene);
    test::expect(pick_actor(scene, sprites, entry.minimum + entry.size * 0.5f) == target, "Edited tree was not pickable at its new position");
    // Valid variant must not partially apply alongside an invalid position.
    for (const auto value : {std::int64_t{2}, std::int64_t{100}, std::int64_t{std::numeric_limits<int>::max()}, std::numeric_limits<std::int64_t>::max()}) {
        EntityEdit invalid{target};
        invalid.properties = {change(world, target, ecs.id<ActorAppearance>(), "variant", std::uint64_t{0}),
                              change(world, target, ecs.id<ActorPosition>(), "hex.x", value)};
        test::expect(!apply(world, invalid).applied && world.inspect(target)->appearance.variant == 2 && notifications == 1,
                     "Invalid position partially applied");
    }
    for (double offset : {1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        EntityEdit invalid{target}; invalid.properties = {change(world, target, ecs.id<ActorPosition>(), "offset.x", offset)};
        test::expect(!apply(world, invalid).applied, "Invalid/non-finite geometry accepted");
    }
    world.map().tiles[world.map().index({3, 0})] = Terrain::sand;
    EntityEdit terrain{target};
    terrain.properties = {change(world, target, ecs.id<ActorPosition>(), "hex.x", std::int64_t{3})};
    test::expect(!apply(world, terrain).applied, "Tree moved onto ineligible terrain");
    EntityEdit invalid{target}; invalid.properties = {change(world, target, ecs.id<ActorAppearance>(), "kind", std::int64_t{9})};
    test::expect(!apply(world, invalid).applied, "Invalid enum accepted");
    invalid.properties = {change(world, target, ecs.id<ActorAppearance>(), "variant", std::uint64_t{3})};
    test::expect(!apply(world, invalid).applied, "Invalid variant accepted");
    invalid.properties = {change(world, target, ecs.id<ActorIdentity>(), "value", std::uint64_t{999})};
    test::expect(!apply(world, invalid).applied, "Identity was editable");
}

void check_boundary_conflicts_and_stale_targets()
{
    Simulation simulation; simulation.settings.speed = 0;
    auto& world = simulation.world(); world.reset(map(), {{{0, 0}, 0, {}}});
    auto& ecs = world.registry(); const auto target = world.actors().front().handle;
    EntityEdit edit{target}; edit.properties = {change(world, target, ecs.id<ActorAppearance>(), "variant", std::uint64_t{1})};
    const auto request = simulation.submit_edit(edit);
    test::expect(world.inspect(target)->appearance.variant == 0, "Submitting writes during GUI iteration");
    simulation.advance_simulation(1);
    test::expect(simulation.clock().ticks() == 0 && world.take_edit_result(request)->applied &&
                 world.inspect(target)->appearance.variant == 1, "Paused edit advanced time or failed");
    // The same stale form must conflict rather than overwrite a changed field.
    test::expect(!apply(world, edit).applied && world.inspect(target)->appearance.variant == 1, "Stale field edit overwrote live state");
    EntityEdit next{target}; next.properties = {change(world, target, ecs.id<ActorAppearance>(), "variant", std::uint64_t{2})};
    const auto first = world.submit_edit(next); const auto second = world.submit_edit(next);
    world.flush_edits();
    test::expect(world.take_edit_result(first)->applied && !world.take_edit_result(second)->applied, "FIFO conflicts were not acknowledged in order");
    const auto fire_before = *world.inspect(target)->tree;
    EntityEdit fire{target}; fire.ignite = IgniteEdit{fire_before, 11};
    test::expect(!apply(world, fire).applied && !world.inspect(target)->tree->burning(), "Invalid coupled timer applied");
    fire.ignite->duration_seconds = 12.1;
    test::expect(!apply(world, fire).applied, "Inconsistent timer boundary accepted");
    fire.ignite->duration_seconds = 16;
    const auto fire_request = simulation.submit_edit(fire);
    simulation.advance_simulation(10);
    test::expect(world.take_edit_result(fire_request)->applied && simulation.clock().ticks() == 0 &&
                 world.inspect(target)->tree->burnout_tick == 16 * 32, "Paused fire command advanced simulation");
    test::expect(!apply(world, fire).applied, "Changed fire snapshot did not conflict");
    simulation.settings.speed = 1;
    simulation.advance_simulation(0.25);
    test::expect(simulation.clock().ticks() == 8 && world.inspect(target)->tree->burning(), "Running edit altered fixed clock cadence");
    EntityEdit stale{target}; stale.properties = {change(world, target, ecs.id<ActorAppearance>(), "variant", std::uint64_t{0})};
    world.destroy(target);
    const auto replacement = world.spawn_tree({{0, 0}, 1, {}}, {});
    test::expect(!apply(world, stale).applied && world.inspect(replacement)->appearance.variant == 1,
                 "Deleted entity edit affected a replacement");
    const auto old_world = replacement;
    world.reset(map(), {{{0, 0}, 2, {}}});
    stale.target = old_world;
    test::expect(!apply(world, stale).applied && !world.inspect_entity(old_world), "Old-world edit affected regenerated entities");
    test::expect(world.inspect_entity(world.actors().front().handle)->components.size() >= 5,
                 "Regeneration did not register reflection again");
}
}
int main()
{
    return test::run("Entity explorer", [] {
        check_inspection();
        check_edits_and_rendering();
        check_boundary_conflicts_and_stale_targets();
    });
}
