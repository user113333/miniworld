#pragma once
#include "actors/tree.h"
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Same versioned ID/generation representation, without actor-only validation.
using EntityHandle = ActorHandle;
using PropertyValue = std::variant<bool, std::int64_t, std::uint64_t, double, std::string>;
struct EnumChoice { std::int64_t value; std::string name; };
struct EntityProperty {
    std::uint64_t component = 0;
    std::string path;
    PropertyValue value;
    std::vector<EnumChoice> choices;
    bool editable = false;
    std::string reason = "No domain edit adapter; inspection only.";
    std::string units;
    std::optional<double> minimum, maximum;
    EntityHandle reference;
};
struct EntityComponent {
    std::uint64_t id = 0;
    std::string name;
    std::string detail;
    std::vector<EntityHandle> references;
    std::vector<EntityProperty> properties;
};
struct EntitySummary {
    EntityHandle handle;
    std::string path;
    std::optional<ActorId> actor_id;
    std::string kind;
    bool disabled = false;
    bool prefab = false;
    EntityHandle parent;
    std::vector<std::string> components;
};
struct EntityInspection {
    EntitySummary entity;
    std::vector<EntityComponent> components;
    std::optional<Tree> tree;
};
struct PropertyEdit {
    std::uint64_t component;
    std::string path;
    PropertyValue expected;
    PropertyValue value;
};
struct IgniteEdit {
    Tree expected;
    double duration_seconds = 16.0;
};
struct EntityEdit {
    EntityHandle target;
    std::vector<PropertyEdit> properties;
    std::optional<IgniteEdit> ignite;
};
struct EntityEditResult {
    std::uint64_t request = 0;
    bool applied = false;
    std::string message;
};
