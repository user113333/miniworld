# Project status

## Completed milestones

Flecs is now the authoritative actor store and executes ordered simulation systems.
Implementation, review and Debug/Release validation are complete and user-approved.

The colony simulation foundation is also complete and approved.
Settlers, houses, jobs, pathfinding, construction and ships are planned below.
Save/load remains a later milestone.

Tree lifecycle simplification is complete: trees
start fully grown, die only from fire, and never regrow naturally.

The Entity Explorer is implemented with full-registry browsing, reflection,
staged domain edits, conflict detection and paused command execution.

## Architecture

| Files | Responsibility |
| --- | --- |
| `src/application.*`, `src/main.cpp` | SDL ownership, events, frame orchestration and entry point |
| `src/actors/` | Plain shared components, tree fire state, actor factory and read-only views |
| `src/simulation/simulation.*` | Generation/settings and the authoritative fixed-step clock |
| `src/simulation/world.*` | Flecs world, component queries, handles, commands, occupancy and ordered systems |
| `src/simulation/entity_*` | General entity inspection, reflected property schemas and atomic validated edit commands |
| `src/simulation/systems/` | Deterministic forest, fire, recovery and keyed random sampling |
| `src/world/` | Terrain/recovery arrays, hex geometry and seeded generation/placement |
| `src/render/` | SDL terrain/actor/particle drawing and shared GLM sprite layout/picking |
| `src/effects/` | Bounded CPU particles and the read-only actor-to-fire-emitter adapter |
| `src/gui/` | All ImGui code, tools, menus, windows, DPI/fonts, backend lifecycle and entity explorer |
| `tests/` | Domain, effects, pixel and ECS regressions; fixed seed/runtime fixtures |

`miniworld_world` links GLM and static Flecs, without SDL or ImGui. Its `World`
owns one registry; trees compose ActorIdentity, ActorPosition, ActorAppearance,
Tree and Selectable components. There is no mutable runtime actor vector in TileMap.
`miniworld_effects` contains particles and GLM scene layout, without Flecs or SDL.
The application adapts actor views into visual effects after world updates.
Keep CMake definitions at the root and leave vendored libraries/assets unchanged.

Stable gameplay IDs drive keyed randomness and depth tie-breaking. Actor handles
carry the full versioned Flecs entity ID plus world generation; deletion/reuse
and regeneration invalidate stale selection/commands. Presentation receives
value views, never persistent pointers into ECS storage. GUI tool state stays in
`src/gui/`, separate from generation and simulation settings.

## Preserved behavior

- Camera starts centered with a 20-unit hex radius. Resizing preserves pan/zoom.
  Simulation's exclusive Camera control toggle (Space, when keyboard is not captured)
  enables left-drag panning and cursor-centered wheel zoom (0.25–8x). Escape exits;
  GUI mouse capture blocks camera input. Disabling the tool retains the view.

- Seeded connected hex island, inland centered origin, water/sand/land/grass.
  Initial density preserves the approved coherent forest layout/variants/offsets.
- Shared 32 Hz clock, 4 Hz world rules, integer 0–50x slider and pause/resume.
  At most 250 ms real time is admitted per frame: 400 ticks/50 world steps at 50x.
  Pause retains fractional time; minimized/restoration intervals are discarded.
- Trees start fully grown and persist without fire. Natural births, aging, growth
  and regrowth are disabled; one tree or charred remnant occupies each tile.
- Queued ignition, six-neighbor staged spread, seeded burn duration, charred remains
  and a steady muted red burning tint. Supplied sprite image data stays unchanged.
- Burned terrain stays charred for half its seeded 90–180 second recovery, becomes
  weathered earth, then restores the original grass/land. Recovery never creates trees.
- World order: snapshot occupancy and ignition intents → existing recovery
  → apply ignition → burnout/remains decay → deferred removal commit → occupancy.
  Flecs systems run explicitly from the fixed clock, without a second real-time clock.
- The independent 4,096-slot particle pool advances at 32 Hz. Flames/smoke/embers
  use stable source IDs, bounded residual tails and grant-based round-robin fairness.
- Actor drawing/picking shares depth order, source alpha masks, snapped anchors and
  smooth sizing proportional to camera zoom. Particle footprints retain integer
  physical-pixel sizes. Foreground actors cover background fire groups.
- Island contains seed, dimensions, initial density and Generate/Regenerate.
  Simulation contains speed/time, pause, live counts and exclusive selection/fire tools.
  View reopens the windows. Escape exits tools; ImGui capture blocks world clicks.
- Entities and View → Entity Explorer open/focus a closable full-registry browser.
  Scene selection opens the same inspector. Search, kind/component filters, stable
  sorting, clipped lists, parent/child navigation and reference links cover actors,
  systems, types, modules, prefabs, disabled entities and builtins.
- Reflected fields show live values; unsupported components remain visible.
  Staged actor edits support hex/ground position, valid tree appearance variants,
  selection and coupled ignition/duration. Identity and raw fire timers stay
  read-only. Trees require empty grass/land and offsets inside the owning hex;
  burning trees/remains cannot move. New reflected components are inspectable
  without bespoke GUI code; editing requires a domain adapter.
- Apply queues a typed FIFO group, validated before mutation at the frame boundary
  before time accumulation, including while paused. Direct World updates flush at
  their start. Changed fields conflict against the captured form; failures keep
  the form for correction. Revert reloads live values; selection changes prompt
  before discarding staged edits. Deleted/reused entities and regeneration clear
  stale forms. Occupancy, Flecs notifications, actor views and effects follow edits.
- Regeneration resets time, actors,
  commands, tools and effects while retaining speed and rejecting old-world clicks.
- All ImGui lifecycle belongs to ApplicationUi. Shutdown and partial startup failures
  release textures/backends/context before the renderer/window/SDL.

## Validation

Use the root Ninja presets:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 4
ctest --preset debug
cmake --preset release
cmake --build --preset release --parallel 4
ctest --preset release
```

All 18 CTest suites pass in Debug and Release; both full application builds pass. The Flecs suite checks 24 updated runtime
checkpoints for three seeds through 400 simulation seconds, including exact actor
fire state and recovery. Forest regressions verify persistence for 1,200 seconds
and no births or regrowth. Existing generation fixtures pass.
The entity explorer suites cover full-registry reflection, scalar/string/enum/GLM
fields, relationships, grouped validation, conflicts, notifications, scene/picking,
paused/running boundaries and stale references. A production GUI smoke test covers
a 5,000-entity clipped list, staged selection protection, Apply/Revert, paused fire,
input capture and regeneration. The SDL software-rendered window was reviewed.
See [tests/README.md](../tests/README.md) for fixtures and coverage.

A headless production-frame smoke check passes window close/reopen, pause/resume,
exclusive tools, captured world clicks, actor selection, paused ignition, 50x fire
particles and regeneration clearing handles/tools/time/effects. Normal shutdown
and missing-font/sprite failures release ImGui and SDL correctly.

## Deferred tasks

- [Settlers, construction and sailing](tasks/settlers.md): draft plan for autonomous
  settlers, inventories, tree cutting, houses/fire, mountains, continuous movement
  and ships. Food rules are open; hunger, health and multiple islands are deferred.
- [Asset management and browser](tasks/asset-management.md): shared content
  catalog/loading, resource staging and an ImGui Assets window with thumbnails,
  search and metadata, accessible from the top bar and View menu.

## Deferred review

- Native monitor/DPI transitions and minimize/restore require desktop review;
  the dummy SDL driver cannot exercise native window transitions.
- Full-scene depth and UI-event/DPI smoke checks are currently temporary harnesses;
  permanent tests cover domain behavior, alpha picking, tint and physical pixels.
- Review subjective pacing at 1–50x and dense fires. Extreme Debug catch-up can be
  noticeable; profile before adding render caches or further abstractions.
- Hash/noise consolidation, map-limit/tuning naming, compiler warnings and broader
  SDL draw-error reporting remain optional follow-up work with seed protection.
