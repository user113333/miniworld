#include "world.h"
#include <algorithm>

namespace {
std::string entity_label(const flecs::world& ecs, ecs_entity_t id)
{
    if (!id) return "None";
    const auto name = ecs.entity(id).path();
    return name.length() ? std::string(name.c_str()) : "Entity #" + std::to_string(id);
}

EntitySummary summary(const World& world, flecs::entity entity)
{
    EntitySummary result;
    result.handle = {entity.id(), world.generation()};
    result.path = entity_label(world.registry(), entity.id());
    if (const auto* actor = entity.try_get<ActorIdentity>()) result.actor_id = actor->value;
    if (const auto* appearance = entity.try_get<ActorAppearance>())
        result.kind = appearance->kind == ActorKind::tree ? "Tree" : "Unknown actor";
    else if (entity.has(flecs::System)) result.kind = "System";
    else if (entity.has<flecs::Component>()) result.kind = "Component";
    else if (entity.has(flecs::Module)) result.kind = "Module";
    else result.kind = "Entity";
    result.disabled = entity.has(flecs::Disabled);
    result.prefab = entity.has(flecs::Prefab);
    if (auto parent = entity.parent()) result.parent = {parent.id(), world.generation()};
    entity.each([&](flecs::id id) { result.components.emplace_back(id.str().c_str()); });
    return result;
}

void reflect_fields(const World& world, ecs_entity_t type, const void* data,
                    ecs_id_t component, const std::string& path,
                    std::vector<EntityProperty>& fields, int depth = 0)
{
    if (depth > 12 || fields.size() >= 256) return;
    const auto& ecs = world.registry();
    const auto type_entity = ecs.entity(type);
    if (const auto* structure = type_entity.try_get<flecs::Struct>()) {
        const auto* members = ecs_vec_first_t(&structure->members, ecs_member_t);
        for (int i = 0; i < ecs_vec_count(&structure->members); ++i) {
            const auto& member = members[i];
            const auto name = path.empty() ? std::string(member.name) : path + "." + member.name;
            if (member.count > 1) {
                EntityProperty field;
                field.component = component; field.path = name;
                field.value = std::string("Unsupported array layout");
                fields.push_back(std::move(field));
            } else {
                reflect_fields(world, member.type, static_cast<const char*>(data) + member.offset,
                               component, name, fields, depth + 1);
            }
        }
        return;
    }
    EntityProperty field;
    field.component = component;
    field.path = path.empty() ? "value" : path;
    // Flecs cursors expose a mutable pointer, but these calls only read it.
    ecs_meta_cursor_t cursor{};
    if (type_entity.has<flecs::Enum>() || type_entity.has<flecs::Primitive>())
        cursor = ecs_meta_cursor(ecs.c_ptr(), type, const_cast<void*>(data));
    if (type_entity.has<flecs::Enum>()) {
        field.value = ecs_meta_get_int(&cursor);
        if (const auto* constants = ecs_get(ecs.c_ptr(), type, EcsConstants)) {
            const auto* values = ecs_vec_first_t(&constants->ordered_constants, ecs_enum_constant_t);
            for (int i = 0; i < ecs_vec_count(&constants->ordered_constants); ++i)
                field.choices.push_back({values[i].value, values[i].name});
        }
    } else if (const auto* primitive = type_entity.try_get<flecs::Primitive>()) {
        switch (primitive->kind) {
        case EcsBool: field.value = ecs_meta_get_bool(&cursor); break;
        case EcsChar: case EcsI8: case EcsI16: case EcsI32: case EcsI64: case EcsIPtr:
            field.value = ecs_meta_get_int(&cursor); break;
        case EcsByte: case EcsU8: case EcsU16: case EcsU32: case EcsU64: case EcsUPtr:
            field.value = ecs_meta_get_uint(&cursor); break;
        case EcsF32: case EcsF64: field.value = ecs_meta_get_float(&cursor); break;
        case EcsString: {
            const char* value = ecs_meta_get_string(&cursor);
            field.value = std::string(value ? value : ""); break;
        }
        case EcsEntity: case EcsId: {
            const auto id = primitive->kind == EcsId ? ecs_meta_get_id(&cursor) : ecs_meta_get_entity(&cursor);
            field.value = static_cast<std::uint64_t>(id);
            if (ecs_is_alive(ecs.c_ptr(), id)) field.reference = {id, world.generation()};
            break;
        }
        default: field.value = std::string("Unsupported primitive"); break;
        }
    } else {
        field.value = std::string("Unsupported layout; no scalar/struct reflection");
    }
    fields.push_back(std::move(field));
}

void edit_policy(const World& world, EntityProperty& field, bool actor)
{
    const auto& ecs = world.registry();
    if (field.component == ecs.id<ActorIdentity>()) {
        field.reason = "Stable gameplay identity is read-only.";
    } else if (field.component == ecs.id<Tree>()) {
        field.reason = "Fire timers/remains are simulation-owned; use Ignite below.";
        if (field.path.find("ticks") != std::string::npos) field.units = "ticks (32 ticks = 1 s)";
    } else if (actor && field.component == ecs.id<ActorPosition>()) {
        field.editable = true;
        field.units = field.path.find("offset") == 0 ? "hex radius" : "hex coordinate";
        if (field.path == "offset.x") { field.minimum = -glm::sqrt(3.0) / 2; field.maximum = glm::sqrt(3.0) / 2; }
        if (field.path == "offset.y") { field.minimum = -1; field.maximum = 1; }
    } else if (actor && field.component == ecs.id<ActorAppearance>()) {
        field.editable = true;
        if (field.path == "variant") { field.minimum = 0; field.maximum = 2; }
    }
    if (field.editable) field.reason.clear();
}
}

bool World::entity_alive(EntityHandle handle) const
{
    return handle && handle.generation == generation_ && registry().is_alive(handle.entity);
}

std::vector<EntitySummary> World::entities() const
{
    std::vector<EntitySummary> result;
    const auto entities = ecs_get_entities(registry().c_ptr());
    result.reserve(entities.alive_count);
    for (int i = 0; i < entities.alive_count; ++i)
        result.push_back(summary(*this, registry().entity(entities.ids[i])));
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.handle.entity < b.handle.entity;
    });
    return result;
}

std::optional<EntityInspection> World::inspect_entity(EntityHandle handle) const
{
    if (!entity_alive(handle)) return std::nullopt;
    const auto entity = registry().entity(handle.entity);
    EntityInspection result;
    result.entity = summary(*this, entity);
    if (const auto* tree = entity.try_get<Tree>()) result.tree = *tree;
    entity.each([&](flecs::id id) {
        EntityComponent component;
        component.id = id;
        component.name = id.str().c_str();
        if (id.is_pair()) {
            for (auto target : {id.first(), id.second()})
                if (target.is_alive()) component.references.push_back({target.id(), generation_});
        } else if (registry().is_alive(id)) component.references.push_back({id, generation_});
        const auto* info = ecs_get_type_info(registry().c_ptr(), id);
        if (!info) {
            component.detail = "Tag or relationship (no data).";
        } else {
            const auto* data = ecs_get_id(registry().c_ptr(), entity.id(), id);
            const auto type = registry().entity(info->component);
            if (data && (type.has<flecs::Struct>() || type.has<flecs::Primitive>() || type.has<flecs::Enum>())) {
                reflect_fields(*this, info->component, data, id, "", component.properties);
                for (auto& field : component.properties) edit_policy(*this, field, result.entity.actor_id.has_value());
                component.detail = "Reflected fields";
            } else component.detail = "Unsupported detail: no reflection for this layout (" + std::to_string(info->size) + " bytes).";
        }
        result.components.push_back(std::move(component));
    });
    // Selectable is an explicitly supported tag adapter, not arbitrary tag editing.
    if (result.entity.actor_id) {
        EntityComponent selection;
        selection.id = registry().id<Selectable>();
        selection.name = "Selection policy";
        EntityProperty property;
        property.component = selection.id; property.path = "selectable";
        property.value = entity.has<Selectable>(); property.editable = true; property.reason.clear();
        selection.properties.push_back(std::move(property));
        result.components.push_back(std::move(selection));
    }
    return result;
}
