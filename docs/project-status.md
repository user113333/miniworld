# Project status

The hex-island project is complete and merged into `master`. Both local feature
branches were deleted. Read `AGENTS.md` and check `git status` before working.
The milestone record is [here](tasks/hex-island-generation.md).

## Current task: colony simulation foundation

Long-term direction: develop this prototype into a colony simulation. The next
task is tracked in [colony-simulation-foundation.md](tasks/colony-simulation-foundation.md).
Phases 1–4 (clock, living forests, fire, particle visuals and recovery) are
implemented, validated and approved. The colony simulation foundation is complete.

1. Implemented: shared fixed-step simulation time, pause and speed controls.
2. Implemented: gradual tree births, grass-weighted forest growth, aging, death and decay,
   using the supplied `tree_growing.png` and `tree_dead.png`.
3. Implemented: simulation-time fire spread, click-to-ignite and particle visuals.
4. Implemented: gradual burned-ground recovery and later tree regrowth.

The plan covers deterministic updates, occupancy, overlapping-sprite picking,
initial-density behavior, reset semantics, performance limits and CTest additions.
The companion [tree-fire particle plan](tasks/tree-fire-particles.md) specifies a
shared bounded CPU pool for pixel flames, rising smoke and embers. Both plans now
use one 32 Hz simulation clock: world rules run every eight ticks (4 Hz), while
particles advance on every tick, sharing pause/speed/reset behavior. Particle
logic and SDL drawing stay separate from fire propagation and cannot affect it.
The two extra sprites are user-supplied assets; their image data must remain unchanged.

Phase 1 adds a domain-only `SimulationClock`, owned by `IslandDemo`. It admits at
most 250 ms of real time per frame at 0–50x speed (whole-number slider steps),
advances 32 Hz base ticks and exposes the eight-tick world cadence. The Island window has a
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

Phase 3 adds the **Ignite trees** tool. Each left-button press picks the frontmost
opaque tree pixel, respecting ImGui capture and physical-pixel sprite layout.
Escape or the checkbox cancels tool mode. Paused clicks queue stable IDs and show
a pending count; ignition starts on the next world update after resume. Regenerate
clears commands/tool mode and rejects old-world clicks even if tree IDs are reused.

Saplings, mature trees and uncharred dead wood burn and can spread to the six
neighbor cells. Spread uses start-of-update fires, combined neighbor pressure and
one deterministic decision per target. Burn-down leaves inert charred remains and
canonical burned terrain. Ordinary age death never scorches ground.

Application owns a shared fixed 4,096-slot CPU particle pool for pixel flames,
smoke and embers. It advances after each base tick's world update, sharing pause,
speed and reset. Effects use an independent seeded stream and cannot change the
world. Drawing groups by ground depth so foreground trees cover background fires;
short residual particles outlive their emitter without retaining tree pointers.

Burning trees also receive a steady muted red tint, independent of particle pool
capacity. It appears only after ignition, freezes with pause and ends at burnout.
Shared sprite texture modulation is restored after each draw; unlit neighbors and
the supplied image data remain unchanged.

Phase 4 advances each scar for its seeded 90–180 simulation seconds. It stays
charred for the first half, then becomes lighter weathered earth; both stages
remain canonical burned terrain and block births. Completion restores the saved
grass/land type and clears the recovery record. Newly restored cells can first
attempt a probabilistic birth on the following world update. Recovery continues
after charred wood decays, and an occupied cell still blocks births after recovery.

Recovery shares the existing world clock and pause/speed/reset policy. Rendering
reads progress without advancing it. No offline progression or new gameplay
systems were added. Review this completed foundation before the next milestone.

The user-approved speed extension raises the per-frame bound to 400 base ticks
and 50 world updates at 50x. Fractional time and fixed-step replay are preserved.

The **Select entities** tool opens a closable **Tree** inspector on opaque sprite
clicks. It shows live ID, hex position, stage, age and fire state. Selection and
ignition tools are mutually exclusive; Escape disables either tool. Empty-ground
clicks clear selection. The inspector follows a stable tree ID and closes when
the tree decays or the world regenerates; it never retains a tree pointer.
Validated with the Debug build, all 15 CTest suites and a headless SDL input smoke
check covering selection gating/UI capture, close/reopen, empty clicks, Escape,
decay/reset/stale clicks and the existing ignition tool.

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
  Initial density applies on generation; natural tree turnover leaves terrain unchanged.
- No colonists, castle, house integration, camera or grid. `house1.png` is unused.

## Code ownership

| Files | Responsibility |
| --- | --- |
| `main.cpp` | Entry point and top-level exception reporting |
| `application.*` | Scoped SDL/ImGui ownership, initialization, events and frame orchestration |
| `application_ui.*`, `island_controls.*` | Menus, controls, font/DPI, origin hint |
| `tree_inspector.*` | Read-only selected-tree window, tracked by ID and world generation |
| `island_demo.*` | World state, settings and generation actions; no UI/rendering |
| `simulation_clock.*` | Fixed-step simulation time, input validation and world cadence |
| `world_simulation.*` | Seeded ecology/fire/recovery, stable IDs and occupancy |
| `actors/` | Plain game data for objects the player can interact with, starting with `tree.h` |
| `hex_grid.h`, `tile_map.h` | Axial geometry and centered odd-row storage |
| `island_generator.*`, `tree_placement.*` | Seeded terrain and forest generation |
| `map_renderer.*`, `tree_renderer.*` | SDL drawing, map fitting and sprite resources |
| `tree_scene.*` | Shared sprite layout, depth ordering and alpha-mask picking; GLM only |
| `particle_system.*` | Bounded seeded CPU effects; GLM only |
| `fire_effects.*`, `particle_renderer.*` | Read-only fire adapter and SDL square drawing |
| `tests/` | Permanent CTest domain/effects/pixel regressions and seed snapshots |

Use the existing SDL3, ImGui, GLM submodules and system FreeType. SDL includes
PNG loading; SDL_image is unnecessary. Root CMake copies required assets next
to the executable. Keep CMake in the root and use the Ninja `debug` preset.
The `miniworld_world` static target shares terrain/forest logic, simulation,
`IslandDemo` and GLM between the application and tests, without SDL or ImGui dependencies.
The separate `miniworld_effects` target contains CPU particles and scene layout.
The world library does not depend on it; application-level code adapts burning
trees into emitter views after world updates.

## UI fixes after project review

Implemented and validated:

1. UI actions run before `fit_map`. A newly generated map uses its own dimensions
   on the same frame. `draw_origin_hint` uses the resulting MapView separately.
2. Island's first-open position uses viewport coordinates, current toolbar height
   and DPI-scaled margins/initial width. Saved ImGui positions remain honored.
   Current frame height is checked because viewport WorkPos can lag by a frame.

Frame order: measure monotonic elapsed time → events → platform/backend NewFrame
→ UI actions → fit current map → resolve world clicks → advance world/effects ticks → inspector → origin
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
cmake --preset release
cmake --build --preset release
ctest --preset release
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
- Fire tools: actual SDL clicks respect ImGui capture, a held button does not
  repeat, Escape exits mode, and paused targets stay unlit until resume. Paused
  particles preserve positions, ages, colors and counters. Regeneration rejects
  saved old-world clicks and clears all effects, including while paused.
- Particle squares: permanent software-surface tests cover exact physical pixel
  footprints at 1x/1.25x/1.5x/2x and nonuniform density, clipping, blend restoration
  and read-only drawing. Directed rounding prevents SDL's software quad path from
  truncating an edge after logical/physical coordinate conversion.
- Recovery: production frames reached weathering at 50x, stayed frozen through
  five long paused frames, resumed by 400 base ticks and regenerated with no scars,
  timers or old particles. Repeated redraws never advanced recovery clocks.

Real-fire captures at 2/6/12/24 seconds were inspected for one cluster and a dense
mass-fire scene. Temporary SDL checks also verified smoke/sprite/flame/ember order,
foreground occlusion, residual-emitter depth and standard alpha blending.

Recovery captures at 24/90/150/240/600 seconds show flat charred/weathered stages,
exact restoration and subsequent regrowth. All 39 cluster scars and all 2,574
mass-fire scars restored by 240 seconds. By 600 seconds, the cluster had 41 birth
events on former scars and 18 living trees there; the mass scene had 991 birth
events and 453 living trees on former scars. These are pacing observations,
not population targets or frozen output fixtures.

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

## Permanent regression tests

`ctest --preset debug` and `ctest --preset release` run fifteen suites:

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
- `world.fire_simulation`: fuel stages, queued ignition, staged neighboring spread,
  combined pressure, burn-down, charred immunity, terrain records and reset.
- `world.recovery`: bounded charred/weathered/restored stages, original terrain,
  corpse-independent progress, blocked same-update births, regrowth/reburn and reset.
- `effects.particle_system`: seeded emission/motion, credits, bounded saturation,
  sustained 3,000-fire/tiny-pool fairness, independent pool slots, pause/reset,
  tails and 50x frame replay.
- `effects.tree_picking`: frontmost opaque pixels, transparent pass-through, all
  life stages, bounds and logical-to-physical DPI mapping.
- `world.fire_integration`: shared per-tick world/emitter/effect order, identical
  400-second fire/recovery replay with disabled/tiny/full visuals, paused commands
  and stale-world rejection.
- `render.particle_renderer`: real CPU flame squares rendered into an SDL memory
  surface, with exact DPI/clipped coverage, blend restoration and unchanged state.
- `render.tree_renderer`: visible burning tint across growing/mature/dead fuel,
  pause-stable draws, no tint leakage to shared-sprite neighbors and burnout reset.
- `render.map_renderer`: exact solid colors for charred/weathered/restored cells,
  unchanged original palette/origin and read-only recovery drawing.

The tests use throwing checks instead of `assert`, so Release keeps validation
enabled. See [tests/README.md](../tests/README.md) for fixture format and usage.
Domain/effects tests use no SDL or window. Renderer tests use SDL's software
memory-surface renderer without a window or display driver. All tests can be omitted
with BUILD_TESTING=OFF.
The original four suites passed in Debug and Release. A deliberately altered fixture was
rejected in Release with its seed/cell location; restoring it made the test pass.
The clock phase builds with the Ninja debug preset and all six suites pass;
the existing seed fixtures are unchanged. The living-forest phase builds with the
same preset and all seven suites pass, including a 1,200-second deterministic
turnover run and coarse/fine frame replay.
The fire phase also passes the Ninja debug build and all thirteen suites, retaining
the original seed fixtures and PNG image data. The completed recovery phase passes
all fifteen suites in Debug and Release, including fire-to-regrowth replay and
active-scar reset. Both full Ninja application builds pass using their presets.
Existing seed fixtures, source PNGs and vendored libraries are unchanged. Native
monitor/minimize transitions and desktop pacing remain manual review items.

Synthetic Debug profiling kept the particle pool at or below 4,096 slots. A worst
clamped 50x frame (400 ticks) took roughly 95/125/444 ms for 500/1,000/6,912 emitters
on this shared host after caching intensity once per emitter/tick. Ordinary frames
admit fewer ticks; extreme mass fires can still cause noticeable Debug catch-up.

Earlier UI/rendering checks used one-off harnesses outside the repository. Those
are still manual/non-permanent coverage; temporary screenshots are not required
project artifacts and should not be relied on after a session reset.

## Deferred review suggestions

These are future work, not authorization to start another milestone:

1. Extend permanent coverage to full-scene depth ordering and UI regeneration/DPI
   behavior. World, alpha picking and particle pixel regressions are now in place.
2. Improve names (`IslandDemo`, `storage_origin`, `Tree::offset`), centralize map
   limits and name generation/forest tuning values. Preserve current seeded output.
3. Consolidate duplicated hash/noise code only with seed-regression protection.
4. Enable warnings on application targets. The review's `-Wall -Wextra -Wpedantic`
   check found a missing `TileMap::trees` initializer warning in the generator;
   the vector is correctly default-initialized, but explicit intent would be clearer.
5. Report SDL draw-call failures. Profile before adding render caches or abstractions.

The UI fixes, permanent world tests and lifecycle refactor leave these suggestions deferred.
Follow AGENTS.md: summarize validation and wait for approval before every commit.
