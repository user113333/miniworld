# Colony simulation foundation — implementation plan

Status: planned only. No implementation has started.
Baseline: `master`, lifecycle refactor `98dc328`; world CTest suite `3f0a536`.
Read `AGENTS.md` and `docs/project-status.md` first in a new session.

This is a new task after the completed hex-island project. The long-term direction
is a colony simulation; this task adds its shared simulation clock and a small
living environment. Do not add colonists, jobs, resources, buildings or gameplay
frameworks yet. Implement only after the user authorizes implementation. Request
approval before each small, logical commit.

## Requirements

1. Simulation time with pause and a speed slider in the Island window.
2. Gradual tree births, growth, aging, death and eventual removal. Grass should
   have a higher birth probability than land under comparable conditions.
3. Use the supplied growing/dead sprites and retain clustered forests, pixel
   scaling, overlaps, depth order and the centered world origin.
4. Flammable trees, manual click-to-ignite tool, gradual nearby propagation and
   eventual fire death, all driven by simulation time.
5. Fire-killed trees leave burned terrain. It recovers gradually and can support
   new trees after recovery. Ordinary age-related death does not scorch terrain.
6. Keep domain logic independent of SDL/ImGui, seeded behavior reproducible, and
   update costs bounded. Preserve the approved initial island/forest layouts.

## Inspection findings

- `Application` owns lifecycle/events/frame orchestration. It currently skips
  drawing while minimized and has no elapsed-time measurement or simulation update.
- `IslandDemo` owns settings and TileMap; Generate/Regenerate replace the world.
- `Tree` currently contains only axial coordinate, mature sprite variant and
  radius-relative ground offset. Trees live in a vector; there are no stable IDs,
  ages, occupancy index, fire state or terrain recovery state.
- `TileMap::tiles` contains four terrain types. `map_renderer.cpp` indexes a
  four-entry color array; adding terrain requires updating this lookup safely.
- `place_trees` builds the initial clustered forest. It clears the tree vector.
  The current density slider calls it immediately: this must not reset a living
  world, ages or fires once simulation exists.
- `TreeRenderer` loads three mature sprites, snaps ground anchors to physical
  pixels, uses integer scaling/nearest sampling, and sorts by actual ground Y.
- Inspected `tree_growing.png` and `tree_dead.png`: both transparent 8x8 canvases,
  compatible with existing anchors. The growing sprite is a small sapling; the
  dead sprite is a sparse trunk/stump. Both are user-supplied assets;
  leave their image data unchanged.
- SDL already loads PNG. No image loader, physics, ECS, or scheduling dependency
  is needed. `miniworld_world` is the correct target for simulation and tests.
- Four permanent CTest suites cover geometry, static generation, placement and
  frozen seed snapshots. They intentionally do not cover live simulation yet.

## Recommended design

### One simulation clock

Use a shared **1/32-second simulation base tick** and an integer tick counter.
`SimulationClock` owns the accumulator. Ecology, fire and recovery update every
eight base ticks with a 0.25-second world step; visual particles update on every
base tick. This refines the original 0.25-second clock proposal to support smooth
fire visuals without introducing a second time source. See the
[particle plan](tree-fire-particles.md) for the companion visual task.
Expose elapsed simulation seconds for the UI. Future colony systems can consume
these same steps without knowing SDL or render FPS. A cadence check is enough;
do not introduce a calendar, generic event bus or subsystem scheduler.

`Application` measures real elapsed time using a monotonic clock. Apply a 0–8x
speed multiplier, default 1x. Zero means pause; offer a nearby explicit Pause/Resume
control or clear 0x label. Slider changes take effect before that frame's advance.

- Clamp real frame deltas to 250 ms; with 8x speed, at most 64 base ticks and
  eight world updates are due per frame. Discard excess stall time rather than
  creating a catch-up spiral.
- Ignore negative/non-finite elapsed input and validate the speed range at the
  clock boundary, not only in ImGui.
- While paused, do not advance ticks, ages, probabilities, flames or recovery.
  Preserve only the existing fractional accumulator, not paused wall time.
- Pause elapsed accumulation while minimized, matching the current inactive-loop
  behavior. Refresh wall-clock timing so restoring cannot fast-forward the world.
- Generate/Regenerate resets elapsed time, accumulator, simulation state and pending
  commands, while retaining the user's chosen speed. No offline progression.
- Test boundary accounting and equal-total frame partitions; do not update the
  world with variable-sized frame deltas or silently lose ordinary fixed steps.

### Small domain model

Extend existing types rather than building an entity framework:

- Tree: stable ID, `growing/mature/dead` stage, age and sampled lifecycle durations,
  remaining burn duration, and whether it is charred. Keep hex, variant and offset.
  Use integer base ticks for durations/deadlines; convert tuning seconds at the
  boundary and align world transitions to eight-tick world-update boundaries.
  Burning is separate from life stage: a sapling, mature tree or uncharred dead
  trunk can burn. Charred remains have no fuel and cannot be reignited.
- Map: keep terrain as the canonical current terrain. Append `Terrain::burned`
  without renumbering existing terrain values. Add bounded per-cell recovery data
  containing the original terrain and recovery duration/progress. Initialize it
  inactive on a new world; no burned cells are generated at time zero.
- Simulation: owns world-update processing, stable-ID allocation and pending ignition
  commands. Maintain a map-sized occupancy array from cell to current tree index.
  Rebuild it after compaction/batches; never retain vector indices or pointers as IDs.
  Allocate initial IDs in cell order and newborn IDs in committed cell order.
- Keep one tree, including a corpse, per tile. Its ground offset remains within
  the owning hex even when its canopy overlaps other tiles.

Recommended modules: `simulation_clock.*` for timing and `world_simulation.*` for
birth/lifecycle/fire/recovery updates. Add small helpers inside these modules as
needed. Keep `IslandDemo` as controller for now; unrelated naming/noise refactors
from the review remain deferred. SDL rendering and picking stay in renderer files.
`IslandDemo` owns the single `SimulationClock` and `WorldSimulation`. Application
supplies real elapsed time after UI actions and drives admitted base ticks one
at a time through the controller: world update at each eight-tick boundary,
then the visual effects update for that base tick. SimulationClock and
WorldSimulation do not depend on effects code. Renderers and UI do not maintain
their own clocks. Speed is plain settings data, not an ImGui dependency.

### Determinism and update order

Use a simulation random stream independent of initial map/forest generation.
Prefer a deterministic keyed sample of world seed, tick, tile/tree ID and event
channel (birth, lifespan, spread, etc.). Do not use wall time, pointer values or
per-frame random draws. Avoid random-consumption dependence on vector erase order.

For gameplay rates, use `p = 1 - exp(-rate * world_dt)` with world_dt = 0.25 s,
rather than a per-frame chance or the finer particle interval.
A fixed seed, configuration and sequence of commands at specified ticks must
produce the same world for the same accepted elapsed time, regardless of render
frame partitioning. Intentionally discarded stall/minimized time is excluded.

Each world update uses a start-of-update occupancy/state snapshot. Collect intents, resolve
conflicts in stable cell/ID order, then apply changes. Newly born, newly ignited or
just removed objects must not recursively trigger more updates in the same tick.
In particular, a new fire can spread only on a later world update: never flood-fill fire.
Fire death takes priority over growth/ordinary age death and marks terrain once.
A newly cleared or restored cell can first attempt a birth on the following tick.

### Tree ecology

Preserve the current mature forest at time zero. Initialize its runtime ages with
seeded staggered values safely below death, preventing a synchronized die-off.
New births start as saplings and use `tree_growing.png`, then select their stable
mature variant among tree1/2/3. Dead trees use `tree_dead.png` until decay removes
both the object and its occupancy. Normal death leaves the underlying terrain alone.

Attempt births only on empty, fully recovered grass/land cells. Apply higher base
rate on grass and the existing coherent forest suitability as a spatial bias; do
not revert to uniform scattering. Births can slowly colonize open suitable ground.
The density slider becomes **Initial tree density**, applied on Generate/Regenerate
only. Speed remains a live setting. Do not run `place_trees` on a running world when
editing that slider, and do not force the live population back to a target count.

Provisional demo tuning, centralized and expressed in simulation units:

| Parameter | Initial value to review |
| --- | --- |
| Sapling growth | Seeded 15–30 s |
| Total age at natural death | Seeded 180–360 s |
| Natural dead-tree decay | Seeded 30–60 s |
| Base birth rate per empty grass/land cell | 0.002 / 0.0005 per second, multiplied by forest suitability |
| Burn duration | Seeded 12–20 s; dead wood can use a shorter duration |
| Spread rate per adjacent susceptible pair | 0.15 per second |
| Charred remains decay | Seeded 15–30 s |
| Burned-ground recovery | Seeded 90–180 s |

These are balancing defaults, not real-world years. Tune visually so changes happen
one tree at a time over seconds/minutes, with quiet periods and persistent forests.
Do not assert that every individual grass trial succeeds more often; compare base
rates and sufficiently many fixed-seed trials under equal habitat conditions.

### Fire and manual ignition

Start with the six axial neighboring cells as the definition of nearby trees.
No wind, smoke simulation, continuous heat field, jumping across gaps or burning
water/sand. Trees are fuel; bare terrain does not spread fire by itself.
Use occupancy lookup for six-neighbor checks, not all-pairs tree comparisons.

For a susceptible target with several burning neighbors, combine their rates and
make one deterministic ignition decision. Queue at most one ignition per target.
An existing fire is not restarted by another ignition command. Burning prevents
normal corpse removal or age death from bypassing the burn-down transition.
At burn completion, change the tree to charred dead, stop its flame, start its
removal timer, and change its tile to burned with a saved recovery terrain.

Add an **Ignite trees** toggle to the Island window, visibly indicating tool mode.
One left-button press chooses one tree; holding the button must not ignite once
per rendered frame. Cancel mode explicitly, e.g. the toggle or Escape.

- Forward SDL events to ImGui as today. Resolve world clicks after current UI
  processing, honoring `io.WantCaptureMouse`; clicking/dragging controls never ignites.
- Pick the visible tree under the cursor, not just the nearest hex center. Reuse
  exactly the sprite destination/anchor/scale math from rendering, test opaque
  source pixels, and traverse the draw list in reverse so the front tree wins.
  Store tiny source alpha masks when loading the five tree sprites; no GPU readback.
- Handle SDL/ImGui logical mouse coordinates and framebuffer scale once. Cover
  overlapping trees, transparent corners, map bounds and high-DPI input in tests.
- Queue stable tree IDs, not pointers or vector positions. Missing or charred
  targets are harmless no-ops. Reset queues, selection and stale world clicks on
  regeneration; use a world-generation number to reject old-world commands.
- While paused, show a pending target/command indicator. Ignition starts on the
  next world update after resume, preserving a genuinely frozen paused world.

Render growing, mature, dead and burning states with the same ground anchors and
depth order. Use the supplied growing/dead sprites unchanged. The companion
[particle plan](tree-fire-particles.md) specifies CPU-based flame, smoke and ember
visuals and supersedes the earlier simple overlay suggestion. Integrate those
effects with each tree before later foreground trees, not as a global overlay.
All motion, emission and flicker use the shared base ticks so they also pause.

### Burned terrain and recovery

Use regular solid-color hexes as before; a new terrain sprite is unnecessary.
A provisional charred brown `#5b4e45` should clearly differ from normal land.
All gameplay queries see `Terrain::burned`, not merely a cosmetic overlay.

Remember the original grass/land type before burning. Recovery uses per-cell,
seeded durations and two visual stages: charred, then lighter weathering earth,
then the original terrain. Each tile remains a flat color, without texture,
spatial gradients or smoothing. Keep the tile classified burned and birth-blocked
through weathering; only completion restores its original type and eligibility.

Start recovery at burn-down. Charred tree remains may coexist with recovery;
occupancy still blocks births until removal. Restore original terrain even if the
corpse is already gone. Natural dead trees never start burn recovery. No new tree
spawns in the same tick that its cell recovers; later births are probabilistic.
Update `terrain_color` and relevant terrain switches when adding the enum value
so the existing four-entry array cannot be indexed out of bounds.

## Integration and efficiency

Application frame order becomes: measure elapsed time → events → ImGui NewFrame
and controls → fit current map → resolve eligible ignition clicks → advance shared
base ticks (world every eight ticks, effects every tick) → origin hint → render
terrain/trees/effects/ImGui → present.
UI regeneration must still happen before fitting and simulation. Simulation cannot
resize the map mid-frame. Minimal world-to-UI accessors expose time/counts/status.

Scan bounded map cells and trees per world update, using occupancy and batched compaction.
Current limit is 96x72: this is small enough without threads, a spatial tree or ECS.
Keep birth candidates/temporary intents reusable if straightforward; profile before
adding caches. Derive age/recovery visuals from state; do not advance timers while
rendering. Existing ordered tree drawing can be extended and shared with picking.

CMake changes stay in the root: simulation sources/tests join `miniworld_world`;
picking/flame/sprite code stays in application/rendering targets. Copy the two
existing extra PNGs alongside the other assets. No vendored changes or new libraries.

## Implementation phases and review gates

1. **Clock and speed controls** — 32 Hz base ticks, 4 Hz world cadence, pause, reset, stall/minimize policy,
   elapsed-time display. Add domain tests; no tree behavior changes yet.
2. **Living forests** — lifecycle metadata, occupancy, seeded incremental births,
   grass weighting, supplied sapling/dead sprites, initial-density semantics.
   Preserve initial layout fixtures and review pacing/appearance before fire.
3. **Fire and ignition tool** — stable-ID commands, precise picking, staged neighbor
   spread, companion particle visuals, fire death and burned terrain. Recovery remains
   the final phase; report that temporary limitation clearly at review.
4. **Recovery and end-to-end validation** — restore terrain over time, resume natural
   births, refine pacing, and finish documentation. No unrelated colony features.

Suggested short commits: `Add simulation time controls`, `Simulate living forests`,
`Add tree fire simulation`, `Recover burned terrain`. Summarize tests and obtain
approval before every commit. Update this plan's progress section after each phase.

## Validation plan

Keep the existing four CTest suites and seed fixtures passing unchanged at time zero.
Do not regenerate fixtures just to accommodate simulation fields. Keep baseline
mature variant IDs and generated coordinates/offsets intact; add lifecycle defaults
or initialization separately. Live-state replay checks are new tests, not replacements.

Add permanent, headless CTest coverage using current throwing checks:

- Clock: pause/resume, speed ratios, equal-total frame partitions, tick boundaries,
  32 Hz/4 Hz cadence, bounded stall handling (64 base ticks/eight world updates),
  reset, and no minimized/offline catch-up.
- Lifecycle: staged growth, staggered initial ages, natural death/decay, zero/full
  occupancy, birth exclusion on sand/water/burned/occupied cells, grass weighting,
  clustered births, stable IDs, and deterministic replay across frame rates.
- Fire: growing/mature/uncharred-dead susceptibility, charred immunity, adjacency,
  gaps, no same-tick chain reaction, combined neighbor pressure, duplicate commands,
  burn duration and eventual death, and order-independent batched outcomes.
- Recovery: mark terrain exactly once, original type preserved, natural deaths do
  not scorch, visual/progress stages, bounded timers, no early respawn, eventual
  restoration and subsequent births, reset clearing every scar and pending command.
- UI/picking: controls consume input, frontmost opaque sprite selected, transparent
  pixels pass through, DPI mapping, paused queued ignition, and stale IDs after reset.

Build and run Debug CTest after each phase; validate Release checks before final
review. Manually inspect normal/accelerated/paused pacing, dense forest overlap,
click tools, dying fires and recovery. Run an extended fixed-seed simulation to
check bounded memory, occupancy consistency and absence of abrupt whole-forest resets.

## Progress and next session

- [x] Inspect architecture, tests and supplied sprite canvases.
- [x] Specify timing, state, input, fire, recovery and test strategy.
- [ ] Phase 1: clock and speed controls.
- [ ] Phase 2: living forests.
- [ ] Phase 3: fire and manual ignition.
- [ ] Phase 4: recovery and final validation.

Only Markdown was changed while preparing this plan. No source, assets, CMake,
fixtures or tests were modified, and no implementation/build was performed.
Next session: inspect current git status (the two new tree PNGs are user assets),
read this plan, run the existing baseline build/tests, and begin Phase 1 only when
implementation is authorized. Defer workers, jobs, inventory, construction, weather,
pathfinding, resource harvesting, save/load and generic simulation frameworks.
