# Flecs expansion: entity explorer and property editing

Status: planned; no implementation yet.

## Goal

Create an application-owned ImGui Entity Explorer for browsing all live Flecs
entities, inspecting their components and changing supported properties. It must
grow with settlers, houses, ships and later simulation features rather than
remain a tree-only inspector.

Flecs stays the authoritative store. The explorer reads live state and submits
validated changes to that same world; it does not maintain an editable copy of
the simulation as a second source of truth.

## Current integration points

- `ActorInspector` shows a selected tree's ID, position, stage, age and fire state
  as read-only values. It cannot browse the world or inspect non-actor entities.
- `World::actors()` and `World::inspect()` require actor components. Flecs also
  contains component definitions, systems and other entities outside those views.
- `ActorHandle` includes the full versioned entity ID and world generation, but
  its current validation also requires `ActorIdentity`. A general entity explorer
  needs equivalent stale-reference protection without that actor-only condition.
- Flecs reflection support is available in the vendored library, but application
  component fields have no registered reflection metadata yet.
- GUI code belongs in `src/gui/`; mutation, validation and ordered execution
  belong in `src/simulation/`. Keep the domain independent of SDL and ImGui.

## Entity browsing

- Provide an **Entities** top-bar button and a checked **View → Entity Explorer**
  menu entry. Both open/focus one closable, reopenable window.
- List live entities with name/path, full Flecs ID and gameplay actor ID/kind
  where present. Unnamed entities need a useful ID-based label.
- Default to gameplay entities, with an obvious **All entities** mode that also
  exposes systems, component/type definitions, modules, prefabs and Flecs builtins.
  Show disabled/prefab state distinctly; filters must not make these impossible
  to discover. Entity browsing must not depend on having `ActorIdentity`.
- Search by name/path or ID, filter by kind/component, and sort deterministically.
  Show counts for the current result and total live entities. Clip long lists.
- Where Flecs parent/child relationships exist, offer hierarchy navigation;
  also provide a flat list covering every entity in the current registry.
- Selecting an entity shows its components, tags and relationship pairs. Allow
  following valid entity references to their targets. Missing names or unsupported
  component layouts must not hide the entity.
- World selection should be able to open/focus the same entity in the explorer.
  Share the underlying inspection data with the actor inspector, or route actor
  inspection into the explorer, rather than duplicating component definitions.

## Reflection and property editors

- Register component/member names and Flecs reflection metadata beside the
  application-owned components in `src/actors/` and relevant simulation modules.
  Re-register it when a new registry is created on regeneration.
- Use generic editors for supported scalars, booleans, enum choices, strings and
  nested fields. Support GLM coordinates/vectors without replacing their domain
  types. Display units, valid ranges and useful labels for simulation time.
- Separate type reflection from edit policy: knowing a field's memory layout
  does not establish how a change should affect the simulation. Domain metadata
  or explicit component adapters declare editable fields and validators.
- Start with useful actor edits: position, valid appearance variants and
  selection state, plus applicable tree/fire properties through domain rules.
  Use the tree schema present at implementation time; do not reintroduce stages
  removed by [tree lifecycle simplification](simplify-tree-lifecycle.md).
- Represent coupled state as one coherent edit. For example, ignition and burn
  duration must obey fire rules instead of exposing unrelated raw timestamp
  writes that can create an impossible burning state.
- Keep identity, entity generation, system scheduling and engine-owned metadata
  read-only. Derived values remain read-only unless a meaningful domain edit is
  defined. Show why a field/component cannot be edited.
- Display unreflected components by name/type information with an unsupported
  detail message. Do not guess field layouts or treat arbitrary memory as data.
- Support future components through metadata/adapters, including settler
  inventories, work state and building/ship properties when those systems exist.
  Do not implement those gameplay systems as part of this task.

## Edit execution and live state

- Stage a selected entity's changes in GUI-owned form state with **Apply** and
  **Revert** controls. Distinguish staged values from live values; selection or
  simulation refresh must not silently overwrite an in-progress edit.
- Submit typed edit commands through a simulation API. GUI code must not retain
  mutable ECS pointers or modify the registry during drawing/iteration.
- Give every target a full versioned Flecs entity ID plus world generation.
  Revalidate at apply time; entity reuse, destruction or regeneration must not
  redirect an old edit to a new entity. Clear stale selection and staged forms.
- Apply edits at a documented simulation boundary. Support applying while paused
  without advancing simulation time, work, movement or fire. Running edits use
  the same validation/order and are acknowledged as applied or rejected.
- Apply a grouped edit atomically. If validation fails, report the reason and
  leave the entity/world unchanged; keep the form available for correction.
- Detect conflicts when a field being edited has changed since the form was
  captured. Report the conflict and allow reloading live values rather than
  silently overwriting simulation progress with a stale component snapshot.
- Position edits must validate bounds, geometry and relevant occupancy/traversal
  rules. Inventory edits must respect quantities/capacity when inventories exist.
  References must point to live, compatible targets. Reject non-finite numbers,
  invalid enum values and inconsistent timers.
- Notify Flecs of successful component changes through supported mutation APIs,
  and refresh dependent occupancy, spatial data, effects and read-only views as
  needed. Editing a tree's position cannot leave the old tile occupied or make
  rendering and picking disagree about its new location.
- Preserve stable IDs and deterministic command order. Explorer browsing alone
  cannot advance the simulation, consume randomness or change query results.

## Scope and integration

Keep window layout, filters, selection and editor widgets in
`src/gui/entity_explorer.*`, forwarded through `ApplicationUi`. Keep general
entity inspection, edit schemas/commands and validation in the simulation layer.
Build definitions remain in root `CMakeLists.txt`; leave vendored Flecs unchanged.

Use the full existing registry, not a separate demo world. Metadata should be
registered once per world lifetime; do not rebuild reflection or deep-copy every
entity's component data each GUI frame. Read details for the selected entity and
reuse lightweight list/query results where appropriate.

The first version edits properties on existing entities. Arbitrary entity
creation/deletion, adding/removing arbitrary component types, scheduler editing,
undo/redo history, save/load and the remote/web Flecs explorer are separate work.
Tags and relationship changes can be enabled individually through domain edit
adapters where they are valid, not through unrestricted registry operations.

## Implementation milestones

1. **Inspection foundation:** general entity references, registry enumeration,
   component/tag/pair details and reflection for current application components.
2. **Explorer window:** top-bar/menu access, search/filter/sort, entity selection,
   hierarchy/reference navigation and integration with world actor selection.
3. **Property editing:** generic field widgets, component-specific adapters,
   Apply/Revert, validation, conflicts and simulation-owned edit commands.
4. **Integration:** paused/running edits, dependent state refresh, stale handles,
   regeneration and coverage; update project status with supported edit policies.

## Acceptance and validation

- Browse every live entity category in the current registry, including unnamed
  entities and entities without actor components. Filter back to gameplay actors.
- Inspect fields, tags and relationships; select an actor in the scene and find
  the same entity. Adding a reflected component does not require another bespoke
  inspector window just to display its supported fields.
- Move a tree to a valid empty location and change a supported property. Verify
  the registry, scene, picking and occupancy all reflect the accepted edit.
- Reject an occupied/out-of-map position or invalid field value without partial
  changes. Show read-only and unsupported properties clearly.
- Apply an edit while paused with unchanged simulation time. While running,
  conflicting live changes are reported and successful edits occur in order.
- Delete/reuse an entity through the simulation or regenerate while its form is
  open. Stale edits cannot affect a replacement entity and the UI remains usable.
- Exercise scalar, enum, vector and coupled-state editors plus validation,
  conflicts and change notifications in domain-focused tests. Browse a large
  list with clipping and verify that GUI input does not activate world tools.
- Run root Debug/Release builds and CTest presets. Visually review window
  navigation, live updates, Apply/Revert, errors and pause/regeneration behavior.
