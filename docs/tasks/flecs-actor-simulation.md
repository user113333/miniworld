# Flecs actor simulation

Make Flecs the central owner of interactive actors and their simulation systems
before starting colonists. Preserve current gameplay, seeded output and visuals.

## Architecture

- A simulation controller owns the fixed-step clock and an ECS-backed game world.
- Flecs owns actor identity, position, appearance and actor-specific components.
  Keep definitions readable under `src/actors/`; create actors through factories.
- Terrain, burned-ground recovery and hex geometry remain grid data in `src/world/`.
  Occupancy indexes actor handles rather than vector indices.
- Ordered Flecs systems implement forest, fire and recovery under `src/simulation/`.
  Domain code depends on GLM/Flecs, never SDL or ImGui.
- Renderers consume read-only actor views under `src/render/`. CPU particles stay
  in the bounded pool under `src/effects/` and cannot affect gameplay.
- All ImGui code stays in `src/gui/`; selection uses actor handles and inspectors
  read current component data through the simulation interface.
- Preserve gameplay IDs for keyed randomness and depth ordering. Runtime handles
  include the full Flecs entity ID and world generation, rejecting deleted/reused
  entities and references from a regenerated world.
- Keep world updates at 4 Hz and the shared base clock at 32 Hz. Execute systems
  explicitly from that clock; Flecs does not introduce a second real-time clock.

## Required world-step ordering

1. Snapshot occupancy and collect ignition/birth intents from the old state.
2. Advance existing terrain recovery.
3. Apply queued/spread ignitions.
4. Advance lifecycle and burn-down, with fire taking priority over death/decay.
5. Delete decayed actors and create births in deterministic cell order.
6. Refresh occupancy and publish actor views.

Structural changes must not create same-step fire chains, births in newly cleared
cells or births on ground restored in the same update. Rendering, selection and
particle capacity must not alter simulation results.

## Implementation

- [x] Integrate the static Flecs target in root CMake; disable its shared library/tests.
- [x] Capture existing runtime replay checkpoints before replacing actor storage.
- [x] Move actor ownership from `TileMap::trees` into Flecs and add shared components,
  actor factories, validated handles, occupancy and read-only views.
- [x] Migrate the controller and ordered forest/fire/recovery systems.
- [x] Adapt rendering, picking, effects, counts and inspectors to actor views/handles.
- [x] Organize domain, render and effects files into their corresponding directories.
- [x] Adapt regression tests and add ECS identity, query and baseline replay coverage.
- [x] Validate Debug/Release builds and CTest, plus UI interaction/reset smoke checks.
- [x] Update the project handoff and architecture guidance.

Colonists, buildings, jobs, relationships, pathfinding and save/load are later
milestones. This task establishes their actor and simulation foundation.

## Acceptance

- Flecs is the sole authoritative actor store, with actual component queries and
  systems driving existing gameplay; no mirrored mutable actor vector remains.
- Initial seed fixtures and captured pre-migration runtime checkpoints match.
- Pause, 0–50x speed, staged fire, recovery, selection, picking, depth sorting and
  regeneration retain their behavior. Stale actor handles cannot target new actors.
- Existing affected tests and new ECS regressions pass in Debug and Release.
- Vendored sources/assets remain unchanged. Commit only after user approval.

## Result and validation

Implemented, reviewed and user-approved for commit. Flecs owns actor components
and runs six explicitly ordered systems. Rendering, effects and GUI consume
read-only views; `Simulation` replaces `IslandDemo` and GUI tools use an exclusive
mode outside domain settings. Source files now follow the directory layout above.

- Full Debug and Release Ninja application builds passed.
- All 16 CTest suites passed in both configurations.
- All 24 pre-migration runtime checkpoints match exactly; the original terrain and
  initial-forest fixtures are unchanged.
- Direct ECS tests cover authoritative component edits, table changes, value views,
  deletion/reuse, generation rejection, commands, pause and 50x fixed-step driving.
- A production-frame headless SDL smoke check passed controls, tool exclusivity,
  UI capture, actor selection, paused fire, 50x particles and generation cleanup.
  Normal shutdown and missing-font/sprite failures release GUI/SDL state.
- `git diff --check` passed. Vendored files, submodule revisions and assets are unchanged.

Existing behavior tests retain editable fixtures in `tests/test_state.h` and
`tests/world_fixture.h`, which apply changes to actual Flecs components before
running systems. Those fixture vectors are not production actor storage.
Native monitor/minimize transitions and subjective pacing remain desktop review,
as documented in `docs/project-status.md`.
