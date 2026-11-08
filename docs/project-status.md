# Project status

The hex-island project is complete and merged into `master`. Both local feature
branches were deleted. Read `AGENTS.md` and check `git status` before working.
The milestone record is [here](tasks/hex-island-generation.md).

## Current task: colony simulation foundation

Long-term direction: develop this prototype into a colony simulation. The next
task is tracked in [colony-simulation-foundation.md](tasks/colony-simulation-foundation.md).
Phases 1 (clock and speed controls) and 2 (living forests) are implemented.
Fire, particles and recovery remain planned.

1. Implemented: shared fixed-step simulation time, pause and speed controls.
2. Implemented: gradual tree births, grass-weighted forest growth, aging, death and decay,
   using the supplied `tree_growing.png` and `tree_dead.png`.
3. Add simulation-time fire spread and an ImGui click-to-ignite tool.
4. Add burned terrain and gradual recovery allowing later tree regrowth.

The plan covers deterministic updates, occupancy, overlapping-sprite picking,
initial-density behavior, reset semantics, performance limits and CTest additions.
The companion [tree-fire particle plan](tasks/tree-fire-particles.md) specifies a
shared bounded CPU pool for pixel flames, rising smoke and embers. Both plans now
use one 32 Hz simulation clock: world rules run every eight ticks (4 Hz), while
particles advance on every tick, sharing pause/speed/reset behavior. Particle
logic and SDL drawing stay separate from fire propagation and cannot affect it.
The two extra sprites are user-supplied assets; their image data must remain unchanged.

Phase 1 adds a domain-only `SimulationClock`, owned by `IslandDemo`. It admits at
most 250 ms of real time per frame at 0–50x speed (whole-number slider steps), advances 32 Hz base ticks and
exposes the eight-tick world cadence for later phases. The Island window has a
speed slider, Pause/Resume (restoring the previous speed) and elapsed simulation
seconds. Pause preserves fractional time; minimized time and the first restored
interval are discarded. Generate/Regenerate resets time and fractional steps,
retains speed and discards pre-generation elapsed time on that frame.

Phase 2 adds `WorldSimulation` at the eight-tick world boundaries. Initial trees
remain mature with unchanged layouts, variants and offsets, plus stable IDs and
seeded staggered ages. Births prefer the existing coherent forest patches, with
grass weighted above land. Saplings grow over 15–30 simulation seconds, natural
death occurs at a total age of 180–360 seconds, and dead trunks decay after
30–60 seconds. Trees and corpses occupy one cell each; newly cleared cells cannot
receive a birth until a later world update. Natural death never changes terrain.

The **Initial tree density** slider applies only on Generate/Regenerate. It does
not replace a living forest. The supplied growing/dead sprites are now loaded
without changing their image data; anchors, pixel scaling and depth ordering stay
the same. Pause freezes lifecycle and births along with the shared clock. Reset
clears the living state, occupancy and ID allocation while retaining speed.

Next: review Phase 2 pacing/appearance before starting fire. The older island task
stays complete.

The user-approved speed extension raises the per-frame bound to 400 base ticks
and 50 world updates at 50x. Fractional time and fixed-step replay are preserved.

## Approved behavior

- Regular pointy-top hexes drawn at runtime, one solid color per terrain tile.
- One connected seeded island: water, coastal sand, brown land, green grass.
- World `(0, 0)` is an inland tile at the exact screen center. Water fills the
  whole background; the finite map border is invisible against it.
- User-supplied `res/map/tree1.png` through `tree3.png`, unchanged 8x8 sprites.
  Small, overlapping forest groups; trees with lower ground positions draw last.
- Seeded initial forest ranking, variants and offsets. Initial density is the
  occupied fraction of eligible grass/land tiles; higher values fill the same patches.
- Nearest tree filtering, integer physical-pixel sprite scaling, snapped anchors.
- Cozette at 13 px with FreeType monochrome hinting; ImGui docking is removed.
- View > Island and View > ImGui demo toggle the windows. Island can be closed.
- Generate chooses a random seed; Regenerate uses the edited seed/dimensions.
  Initial density applies on generation; living forests evolve without terrain changes.
- No castle, house integration, camera, grid, or gameplay. `house1.png` is unused.

## Code ownership

| Files | Responsibility |
| --- | --- |
| `main.cpp` | Entry point and top-level exception reporting |
| `application.*` | Scoped SDL/ImGui ownership, initialization, events and frame orchestration |
| `application_ui.*`, `island_controls.*` | Menus, controls, font/DPI, origin hint |
| `island_demo.*` | World state, settings and generation actions; no UI/rendering |
| `simulation_clock.*` | Fixed-step simulation time, input validation and world cadence |
| `world_simulation.*` | Stable IDs, occupancy, seeded births and tree lifecycles |
| `hex_grid.h`, `tile_map.h` | Axial geometry and centered odd-row storage |
| `island_generator.*`, `tree_placement.*` | Seeded terrain and forest generation |
| `map_renderer.*`, `tree_renderer.*` | SDL drawing, map fitting and sprite resources |
| `tests/` | Permanent CTest world regressions and readable seed snapshots |

Use the existing SDL3, ImGui, GLM submodules and system FreeType. SDL includes
PNG loading; SDL_image is unnecessary. Root CMake copies required assets next
to the executable. Keep CMake in the root and use the Ninja `debug` preset.
The `miniworld_world` static target shares terrain/forest logic, simulation,
`IslandDemo` and GLM between the application and tests, without SDL or ImGui dependencies.

## UI fixes after project review

Implemented and validated:

1. UI actions run before `fit_map`. A newly generated map uses its own dimensions
   on the same frame. `draw_origin_hint` uses the resulting MapView separately.
2. Island's first-open position uses viewport coordinates, current toolbar height
   and DPI-scaled margins/initial width. Saved ImGui positions remain honored.
   Current frame height is checked because viewport WorkPos can lag by a frame.

Frame order: measure monotonic elapsed time → events → platform/backend NewFrame
→ UI actions → fit current map → advance simulation ticks → origin
hint → ImGui::Render → water clear → terrain → trees → ImGui → present.
Map drawing applies framebuffer scale once; ImGui's backend handles its own scale.

## Application lifecycle

`Application` owns one application session. `main` creates it and calls `run`;
initialization, event polling, and frame drawing are separate private methods.
World state is created after graphics/assets are ready. Generation and UI logic
remain in their existing modules; the clock phase adds timing to the frame order above.

The destructor handles both normal shutdown and partial initialization failures:
tree textures → initialized ImGui backends → context → SDL renderer → window → SDL.
Readiness flags guard incomplete startup; top-level exceptions unwind the owner
before `main` reports failure. No cleanup paths are duplicated in `main`.

Validated with the Ninja build and all four CTest suites. Isolated headless SDL
checks verified normal quit events, actual entry-point startup/signal-to-quit,
and startup failures for missing font/tree assets and invalid video/render drivers.
After application destruction, SDL was uninitialized and the ImGui context null.
These lifecycle smoke checks are not yet part of the permanent world-only suite.

## Validation and reproduction

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./cmake-build-debug/miniworld
```

Passed: Ninja debug build and `git diff --check`. Targeted headless SDL software
checks executed the production frame code with simulated mouse input:

- Click Regenerate after changing 48x32 to 96x72, then to 12x12. All tile corners
  fit the viewport immediately on each button-release frame; origin stays centered.
- Fresh Island placement at 100%, 150%, 200%, 300% simulated content scale stays
  below the toolbar. At 300%, its top is 93 px and toolbar bottom is 57 px.
- A saved Island position `(150, 180)` remains unchanged at all tested scales.
- Clock controls: actual ImGui Pause clicks freeze the same frame; Resume restores
  3x speed, and slider endpoints 0x/8x apply immediately. Generate/Regenerate
  discard a 10-second pre-reset delta, retain 8x, and finish at zero ticks; the
  next 250 ms frame advances exactly 64 ticks.
- Living forests: initial-density edits preserve live IDs, ages and time; 0%/100%
  applies only on regeneration. Pause and speed changes affect ages in the same
  frame. All five stage sprites match their source pixels at 1x/1.25x/1.5x/2x
  render scale; missing-asset initialization fails cleanly and can be retried.

Ten-minute forest captures retain clear patches after strengthening habitat
weighting. Across seeds 74129, 0 and 42, 64–72% of living trees have at least two
living neighbors at 600 seconds. For seed 74129, population is 101 at time zero,
108 at 120 seconds, 55 at 300 seconds and 80 at 600 seconds, with no more than
two births, deaths or removals per quarter-second world update.

For desktop review, repeat dimension changes and check first-open placement with
fresh ImGui settings, then verify an existing saved position is not reset. Native
monitor transitions still need manual checking; simulated scale is not a substitute.
Native minimize/restore also needs desktop checking: the SDL dummy driver used
for the clock UI smoke check cannot minimize windows. Domain tests cover inactive
time and the application loop discards minimized/restoration intervals.

## Permanent world tests

`ctest --preset debug` runs seven suites:

- `world.hex_grid`: centered axial/storage round trips, bounds, neighbor reciprocity,
  regular geometry and identical shared corners for adjacent hexagons.
- `world.island_generator`: valid/invalid dimensions, deterministic seeds, water
  border, connected inland origin, coastal sand and representative seed variation.
- `world.tree_placement`: unique eligible occupancy, deterministic variants and
  offsets, unchanged terrain, density clamping/endpoints, stable forest growth,
  empty/sparse maps and grouped placement across fixed reference seeds.
- `world.seed_fixtures`: frozen terrain, positions, sprite variants and ground
  offsets for three known seeds. These protect approved output during refactors.
- `world.simulation_clock`: tick boundaries/cadence, speed validation, equal-total
  frame partitions, pause, bounded stalls, inactive time and reset.
- `world.island_demo`: controller timing, speed restoration, Generate/Regenerate
  resets, initial-density semantics and live replay across frame partitions.
- `world.world_simulation`: initial layout preservation, lifecycle boundaries,
  occupancy and stable IDs, weighted/clustered births, order-independent replay
  and bounded long-running forests.

The tests use throwing checks instead of `assert`, so Release keeps validation
enabled. See [tests/README.md](../tests/README.md) for fixture format and usage.
Tests use no graphics driver or window and can be omitted with BUILD_TESTING=OFF.
The original four suites passed in Debug and Release. A deliberately altered fixture was
rejected in Release with its seed/cell location; restoring it made the test pass.
The clock phase builds with the Ninja debug preset and all six suites pass;
the existing seed fixtures are unchanged. The living-forest phase builds with the
same preset and all seven suites pass, including a 1,200-second deterministic
turnover run and coarse/fine frame replay. Release validation for the new simulation
work remains part of the foundation's final review.

Earlier UI/rendering checks used one-off harnesses outside the repository. Those
are still manual/non-permanent coverage; temporary screenshots are not required
project artifacts and should not be relied on after a session reset.

## Deferred review suggestions

These are future work, not authorization to start another milestone:

1. Extend permanent regression coverage to renderer pixel/depth ordering and UI
   regeneration/DPI behavior. World generation and placement tests are in place.
2. Improve names (`IslandDemo`, `storage_origin`, `Tree::offset`), centralize map
   limits and name generation/forest tuning values. Preserve current seeded output.
3. Consolidate duplicated hash/noise code only with seed-regression protection.
4. Enable warnings on application targets. The review's `-Wall -Wextra -Wpedantic`
   check found a missing `TileMap::trees` initializer warning in the generator;
   the vector is correctly default-initialized, but explicit intent would be clearer.
5. Report SDL draw-call failures. Profile before adding render caches or abstractions.

The UI fixes, permanent world tests and lifecycle refactor leave these suggestions deferred.
Follow AGENTS.md: summarize validation and wait for approval before every commit.
