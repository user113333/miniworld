# Project status

## Completed milestones

Flecs is now the authoritative actor store and executes ordered simulation systems.
Implementation, review and Debug/Release validation are complete and user-approved.

The colony simulation foundation is also complete and approved.
All three phases of the approved settlers task are implemented.
Construction, houses and ships remain deferred.
Save/load remains a later milestone.

Tree lifecycle simplification is complete: trees
start fully grown and never regrow naturally. Settlers can now harvest mature trees.

The Entity Explorer is implemented with full-registry browsing, reflection,
staged domain edits, conflict detection and paused command execution.

Asset management is implemented with a shared catalog/texture cache, complete
resource staging and an Assets browser for the actual content.

## Settlers: logic phase

Phase 1 of [Settlers](tasks/settlers.md) is implemented. Simulation regeneration
creates three seeded settlers at distinct walkable positions, with fixed DNA,
500 happiness and empty eight-slot inventories. World fixtures remain tree-only;
`Simulation` populates settlers after resetting the forest. Settlers do not occupy
static tree cells. Their versioned handles invalidate on deletion/regeneration.

`src/actors/settler.*` holds DNA, actions, thoughts and inventory; bounded radius-ten
A* and world/hex conversion live in `src/world/navigation.*`; autonomous behavior
lives in `src/simulation/systems/settlers.cpp`. The manual Flecs settler system runs
on every shared 32 Hz tick after any due 4 Hz fire/recovery step. Routes are reused,
follow adjacent cell centers with continuous offsets, and cancel safely when
terrain or targets become invalid. Queued position edits also cancel old routes.
All unrecognized terrain types default to impassable for future mountains.

Weighted decisions choose rest, wander, cutting and planting. Idle durations track
mean active durations to target 80% elapsed idle time (four seeded hour-long tests
measured 79.5%). Tree work validates again at completion: one harvest awards at
most two seeds/four logs, overflow is discarded, and occupied planting cancels
without consuming a seed. Planted saplings alone join the fixed-step growth query
for exactly sixty seconds; initial trees remain mature and never reproduce.
Thoughts occur every 10–30 simulation seconds, update happiness immediately and
replace ten-second text timers. Zero happiness deletes the settler and inventory.
Thresholds, penalties, radii and shared speed are in `SettlerTuning`.

## Settlers: rendering phase

Phase 2 is implemented using the user-supplied `lib/cute/cute_aseprite.h`, unchanged.
The actual asset is `res/minifolks/villagers2/MiniLumberjack.aseprite` (not
`MiniLuberjack`): 32×32, 45 frames, with all required tags present. The application
texture cache owns decoded RGBA frames, alpha masks and tag durations; consumers
borrow textures until shutdown. The Assets browser still has no static Aseprite
preview, but reports runtime animation loading and loaded dimensions accurately.

`src/render/settler_renderer.*` owns presentation-only animation state. Both idle
clips are chosen randomly at clip boundaries; walk follows movement, work plays
while cutting after arrival, and death plays once for 0.9 simulation seconds after
actor removal. These transient visuals retain only a ground position, animation
and facing, with no inventory, actor or corpse. Regeneration clears them. Frame
durations use the shared simulation tick, so pause and speed apply consistently.
Settlers share tree/particle depth order, snapped ground anchors, nearest sampling,
camera pan/zoom and foot-preserving horizontal flips. Source pixels are unchanged.

Windowless thought bubbles live entirely in `src/gui/thought_bubbles.*`, using the
same head/ground layout and the simulation's remaining thought timer. They follow
camera/movement, expire with simulation time, and introduce no input capture.
A software-rendered scene with active bubbles was visually reviewed; deterministic
rendering checks cover tag timing, random idle variants, pause, facing, camera
scaling, death cleanup, regeneration, bubble visibility and overlapping tree depth.

## Settlers: interaction phase

Phase 3 adds alpha-mask scene picking for settlers in the same depth order as trees,
including current animation frames, facing, zoom and framebuffer scale. Dead visual
remnants are not selectable. Selection is shared with Entity Explorer; selecting a
settler also opens the Settlers window. View → Settlers reopens it, and Simulation
shows a live population count. The window displays happiness, DNA/preferences,
action, thought and all eight inventory slots.

Settlers have been Flecs entities since phase 1. Their factory now lives alongside
actor data in `src/actors/settler.*` and gives them searchable `Settler <actor ID>`
Flecs names. Entity Explorer now includes a Settler kind filter, searches kind names,
and displays read-only settler component fields, thoughts, DNA and inventory values
instead of an opaque layout. Both inspectors use the same versioned entity handle.

Inventory drafts apply atomically through `EntityEdit` with an expected inventory
snapshot. Domain validation rejects invalid item types, quantities outside 0–10 and
nonzero empty slots; zero quantities canonicalize to empty. Concurrent harvesting,
planting or another edit conflicts rather than overwriting live inventory. Revert
reloads live slots; the shared selection-discard prompt also protects inventory
drafts. Deleted settlers and regeneration clear stale forms and requests safely.
If an edit removes the last seed during planting, the action cancels on the next
32 Hz tick before further movement/work, clears its route and waits one simulation
second before deciding again. Regression coverage includes both travel and work,
a remaining-seed control, no tree creation and preservation of unrelated inventory.
Both full application builds and all four focused settler suites pass in Debug and
Release after this fix; the full 25-suite baseline was validated at checkpoint 3/3.

Spawn settler is an exclusive tool in Simulation and Settlers. It queues the exact
clicked position and world generation in the same FIFO as entity edits. Commands
flush at the existing frame boundary, including while paused; water, unknown future
terrain, out-of-map/invalid positions and old-world clicks are rejected. New settlers
receive seeded DNA, 500 happiness and empty inventory. GUI capture blocks spawning;
Escape/cancel and regeneration exit the tool. Successful spawns select the new Flecs
entity. Spawning never changes tree occupancy or blocks other settlers.

No automatic replacements or deferred gameplay was added.

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
| `src/content/` | Resource-relative catalog, file metadata/status and font byte access; no SDL or ImGui |
| `src/render/` | SDL terrain/actor/particle drawing, shared PNG/Aseprite texture cache and GLM layout/picking |
| `src/effects/` | Bounded CPU particles and the read-only actor-to-fire-emitter adapter |
| `src/gui/` | All ImGui code, tools, menus, windows, DPI/fonts, backend lifecycle, entity explorer and asset browser |
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
- Initial trees start fully grown and persist unless burned or harvested. Natural
  births, aging and regrowth are disabled; only planted trees grow. One tree or
  charred remnant occupies each tile.
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
- Assets and View → Assets open/focus a shared catalog browser with folders,
  global search, type/name sorting, filters, keyboard navigation and adjustable
  clipped thumbnail rows. Hidden selections retain their stable resource key.
  PNG previews preserve aspect, pixel sampling and transparency; unsupported
  formats expose metadata and optional preview failures stay in the window.
- Application owns the catalog and texture cache. Resources resolve from the
  executable's staged `res/`, independent of working directory; complete staging
  removes stale files on configure/build. The catalog scans once, PNGs and Aseprite frames decode on
  demand into shared textures/masks, and regeneration retains presentation assets.
  GUI-owned font bytes outlive the atlas. Consumers detach before texture cleanup,
  and textures are freed before their SDL renderer, including partial startup.
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

All 25 CTest suites pass in Debug and Release; both full application builds pass. The Flecs suite checks 24 updated runtime
checkpoints for three seeds through 400 simulation seconds, including exact actor
fire state and recovery. Forest regressions verify persistence for 1,200 seconds
and no births or regrowth. Existing generation fixtures pass.
The entity explorer suites cover full-registry reflection, scalar/string/enum/GLM
fields, relationships, grouped validation, conflicts, notifications, scene/picking,
paused/running boundaries and stale references. A production GUI smoke test covers
a 5,000-entity clipped list, staged selection protection, Apply/Revert, paused fire,
input capture and regeneration. The SDL software-rendered window was reviewed.
Catalog/cache/browser suites cover resource keys, source-only metadata, font
bytes, decode/texture/mask reuse, optional/required failures, partial startup,
search/filter/sort, selection, navigation, clipping and GUI input capture. Normal
build staging add/remove checks and Debug/Release launches from outside the
repository pass. Software-rendered previews and synthetic 1.5× DPI were reviewed.
See [tests/README.md](../tests/README.md) for fixtures and coverage.

The settler interaction suite verifies actual Flecs membership, searchable entity
summaries/component inspection, eight-slot atomic edits, conflicts/FIFO ordering,
invalid values, paused commands, exact spawn offsets, terrain rejection and stale
handles/generations. The production GUI suite drives inventory widgets, Apply/Revert,
selection protection, capture, click-to-spawn, deletion and regeneration. Picking
checks compare source alpha and overlapping tree depth at 1× and 1.5× framebuffer
scales. The Settlers window and eight-slot layout were reviewed via software rendering.

A headless production-frame smoke check passes window close/reopen, pause/resume,
exclusive tools, captured world clicks, actor selection, paused ignition, 50x fire
particles and regeneration clearing handles/tools/time/effects. Normal shutdown
and missing-font/sprite failures release ImGui and SDL correctly.

## Deferred tasks

- [Deferred settler features](tasks/settlers.md): ships, multiple islands, settler
  fire behavior, houses, mountains and reproduction remain outside the completed
  three-phase implementation scope.

## Deferred review

- Remaining coverage gaps from the focused settlers review (2026-11-09): idle-time
  pacing on generated islands and alpha picking of mirrored walk/work frames
  (current picking checks use an unflipped idle frame). The planting cancellation
  finding is fixed; its regression failed before the fix and passes afterward.
- Deferred settler features now have six short task files linked from
  [Settlers](tasks/settlers.md#deferred-tasks-do-not-implement-now); the added house
  preference/unhappy-departure behavior is preserved in [Houses](tasks/houses.md).

- Native monitor/DPI transitions and minimize/restore require desktop review;
  the dummy SDL driver cannot exercise native window transitions.
- Full-scene depth and UI-event/DPI smoke checks are currently temporary harnesses;
  permanent tests cover domain behavior, alpha picking, tint and physical pixels.
- Review subjective pacing at 1–50x and dense fires. Extreme Debug catch-up can be
  noticeable; profile before adding render caches or further abstractions.
- Hash/noise consolidation, map-limit/tuning naming, compiler warnings and broader
  SDL draw-error reporting remain optional follow-up work with seed protection.
