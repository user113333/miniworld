# Project status

## Current task: Flecs actor simulation

The migration is tracked in [flecs-actor-simulation.md](tasks/flecs-actor-simulation.md).
Flecs is now the authoritative actor store and executes ordered simulation systems.
Implementation, review and Debug/Release validation are complete and user-approved.

The previous [colony simulation foundation](tasks/colony-simulation-foundation.md)
is complete and approved. Colonists, houses, jobs, pathfinding, construction and
save/load remain later milestones.

## Architecture

| Files | Responsibility |
| --- | --- |
| `src/application.*`, `src/main.cpp` | SDL ownership, events, frame orchestration and entry point |
| `src/actors/` | Plain shared components, tree lifecycle data, actor factory and read-only views |
| `src/simulation/simulation.*` | Generation/settings and the authoritative fixed-step clock |
| `src/simulation/world.*` | Flecs world, component queries, handles, commands, occupancy and ordered systems |
| `src/simulation/systems/` | Deterministic forest, fire, recovery and keyed random sampling |
| `src/world/` | Terrain/recovery arrays, hex geometry and seeded generation/placement |
| `src/render/` | SDL terrain/actor/particle drawing and shared GLM sprite layout/picking |
| `src/effects/` | Bounded CPU particles and the read-only actor-to-fire-emitter adapter |
| `src/gui/` | All ImGui code, tools, menus, windows, DPI/fonts, backend lifecycle and actor inspector |
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

- Seeded connected hex island, inland centered origin, water/sand/land/grass.
  Initial density preserves the approved coherent forest layout/variants/offsets.
- Shared 32 Hz clock, 4 Hz world rules, integer 0–50x slider and pause/resume.
  At most 250 ms real time is admitted per frame: 400 ticks/50 world steps at 50x.
  Pause retains fractional time; minimized/restoration intervals are discarded.
- Growing/mature/dead trees, staggered seeded initial ages, habitat-weighted births,
  natural death/decay and one tree/corpse per tile. Natural death leaves terrain intact.
- Queued ignition, six-neighbor staged spread, seeded burn duration, charred remains
  and a steady muted red burning tint. Supplied sprite image data stays unchanged.
- Burned terrain stays charred for half its seeded 90–180 second recovery, becomes
  weathered earth, then restores the original grass/land. Births wait until a later
  step; occupancy still blocks them after recovery.
- World order: snapshot occupancy and ignition/birth intents → existing recovery
  → apply ignition → lifecycle/burnout → deferred removal/birth commit → occupancy.
  Flecs systems run explicitly from the fixed clock, without a second real-time clock.
- The independent 4,096-slot particle pool advances at 32 Hz. Flames/smoke/embers
  use stable source IDs, bounded residual tails and grant-based round-robin fairness.
- Actor drawing/picking shares depth order, source alpha masks, snapped anchors and
  integer physical-pixel scaling. Foreground actors cover background fire groups.
- Island contains seed, dimensions, initial density and Generate/Regenerate.
  Simulation contains speed/time, pause, live counts and exclusive selection/fire tools.
  View reopens either window. Escape exits tools; ImGui capture blocks world clicks.
- Selecting a tree opens a read-only live inspector. Empty clicks clear selection;
  actor deletion or regeneration closes it. Regeneration resets time, actors,
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

All 16 CTest suites pass in Debug and Release; both full application builds pass. The new Flecs suite matches all 24 pre-migration
runtime checkpoints for three seeds through 400 simulation seconds, including
exact actor state, fire, recovery and regrowth. Existing generation fixtures pass.
See [tests/README.md](../tests/README.md) for fixtures and coverage.

A headless production-frame smoke check passes window close/reopen, pause/resume,
exclusive tools, captured world clicks, actor selection, paused ignition, 50x fire
particles and regeneration clearing handles/tools/time/effects. Normal shutdown
and missing-font/sprite failures release ImGui and SDL correctly.

## Deferred review

- Native monitor/DPI transitions and minimize/restore require desktop review;
  the dummy SDL driver cannot exercise native window transitions.
- Full-scene depth and UI-event/DPI smoke checks are currently temporary harnesses;
  permanent tests cover domain behavior, alpha picking, tint and physical pixels.
- Review subjective pacing at 1–50x and dense fires. Extreme Debug catch-up can be
  noticeable; profile before adding render caches or further abstractions.
- Hash/noise consolidation, map-limit/tuning naming, compiler warnings and broader
  SDL draw-error reporting remain optional follow-up work with seed protection.
