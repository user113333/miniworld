# Tree-fire particles — implementation plan

Status: CPU pool, drawing and real-fire integration implemented; desktop review remains.
Baseline: `master`, application lifecycle refactor `98dc328`.
Read `AGENTS.md`, `docs/project-status.md`, and the
[colony simulation plan](colony-simulation-foundation.md) first.

This is the visual companion to the planned tree/fire simulation. Foundation
Phase 1 now provides simulation time; burning-tree state is still planned.
Implement this task only after the clock and burning-tree state are available and implementation is
authorized. Obtain approval before each commit. No external libraries or assets.

## Goal and scope

A lightweight CPU particle system shared by all burning trees:

- Sharp, flat-colored squares: animated yellow/orange/red flames, occasional
  rising smoke, and small bright embers with a perceived glow.
- Natural variation in spawn location, velocity, size and lifetime.
- Emission ramps up after ignition and fades before burnout.
- One fixed-capacity pool; hundreds of fires degrade gracefully at capacity.
- Everything uses simulation time, including movement, emission, color changes,
  intensity, aging and death. Pause freezes effects; speed scales them together.
- Visuals do not cause ignition, damage, death or terrain changes.

Exclude bloom, soft sprites, shaders, GPU simulation, lighting, collisions, wind,
heat diffusion, new gameplay controls and a general-purpose effects framework.

## Architecture inspection

These findings describe the pre-foundation baseline. Phase 1 has since added the clock.

- `Application::draw_frame` currently orchestrates UI, map fitting, SDL world
  drawing and ImGui. No elapsed-time update or particle system exists yet.
- `TreeRenderer` already provides crisp integer physical-pixel sprite scaling and
  bottom-center ground anchors. It sorts actual snapped ground Y, then X.
- `MapView` supplies the screen location of world origin and a uniform world radius.
  World +Y projects downward; upward particle velocity therefore has negative Y.
- `map_renderer.cpp` already uses SDL_RenderGeometry. Flat colored quads need no
  textures, atlas, PNG loader or additional dependency.
- The simulation plan's original 0.25-second world step would make particle motion
  visibly choppy. Its clock section is refined below and updated in that document.
- The earlier procedural flame-overlay suggestion is superseded by this plan;
  maintain one fire visual implementation, not two overlapping systems.

## Shared simulation time

Use **one 1/32-second simulation base tick** (32 Hz). Ecology, fire propagation
and recovery still run every eight base ticks, with a 0.25-second world step.
Particles update every base tick. This is a small cadence check, not a scheduler.

`IslandDemo` owns the shared SimulationClock and WorldSimulation. Application
feeds admitted real elapsed time through the controller, then drives base steps
one at a time. At a world boundary, first update the world, then refresh the
read-only fire emitter views, then update particles for the same base tick.
Between world boundaries, reuse the emitter views and derive continuous burn
progress from their simulation start/end ticks. Never advance all world steps
first and then apply the final fire snapshot to the entire elapsed interval.

- Keep the foundation's 0–50x speed, pause, reset and minimized/stall policies.
- A maximum admitted real delta of 0.25 seconds at 50x produces at most **400 base
  steps and 50 world updates per frame**. Normal fractional time stays accumulated.
- No SDL_GetTicks, ImGui DeltaTime, wall-clock randomness or second effects clock
  inside particle code. Do not multiply by speed again after consuming sim steps.
- Pause runs zero base steps: pool contents, color, emission credit, random
  counters and scheduling cursor stay unchanged. Rendering remains read-only.
- No render interpolation in the first version: 32 Hz simulation motion, combined
  with pixel snapping, is adequate for this small pixel-art effect.
- Reset/Generate/Regenerate clears particles, emitter credits, IDs, visual random
  counters and scheduling state, including residual smoke. No old-world effects.

This clock refinement is implemented in foundation Phase 1. Gameplay probabilities
continue to use the 0.25-second world interval, not the finer visual interval.

## Minimal ownership and interfaces

Two small modules are sufficient:

| Module | Responsibility |
| --- | --- |
| `particle_system.*` | CPU state, pool, emission, intensity and fixed-step integration; GLM/std only |
| `particle_renderer.*` | World-to-screen projection, square geometry, color/blend state, read-only drawing; SDL only |

Application owns the visual system and renderer. WorldSimulation does not include
these modules. An adapter reads burning-tree state and creates plain emitter
views: stable tree/world-generation ID, ground position, life-stage visual height,
and ignition/burnout simulation ticks. No tree pointers, vector indices or SDL
objects are stored in particles. No generic event bus is required.

Fire views are refreshed on world updates/reset, not rebuilt by scanning trees
for every particle. Sort emitter IDs only when that source list changes, then
reuse the order for bounded per-step scheduling. Particle allocation and random sampling cannot consume the
gameplay random stream. Changing effects capacity or disabling visuals must leave
the world replay unchanged.

CMake stays in the root. A small `miniworld_effects` target can hold the GLM-only
CPU module and headless tests; the SDL renderer belongs to the application target.
Reuse the existing CTest helper and throwing checks. Do not change vendored code.

## Particle and emitter data

Each particle has the required position, velocity, color, size, age and lifetime:

- `glm::vec2 position`, `velocity`: world/hex-radius coordinates and units per
  simulation second. These never change when the window or DPI changes.
- RGBA color, square size, age and positive lifetime in simulation seconds.
- Kind (`flame`, `smoke`, `ember`), emitter ID, stable birth ordinal and stored
  emitter ground/depth anchor. Tiny optional per-kind acceleration is enough.
- Size is in source-art pixels, normally 1–2, not an antialiased world-space disc.

Emitter state holds fractional emission credits per kind and deterministic birth
counters. Store it in a reused list keyed by stable IDs, bounded by the maximum
map population (96x72 cells). Keep an inactive emitter's anchor only while residual
particles need it; clean it up after its last particle expires.

## One bounded pool

Start with **4,096 shared particle slots** for all kinds and trees. Allocate once:
a dense fixed array plus active count is sufficient. Spawn into the active prefix;
remove expired particles with swap-with-last. No per-particle heap allocation,
linked lists, per-tree particle pools or frame-driven resizing.

- Iterate safely after swap removal so the swapped particle is not skipped.
- Update costs are O(active particles + emitters) per base tick. Maximum catch-up
  work is bounded by 400 steps; no all-pairs particle/tree checks.
- At capacity, drop cosmetic spawn requests instead of allocating or delaying
  gameplay. Do not kill arbitrary living particles to make room.
- Serve flame requests before smoke/embers. Advance the stable-ID round-robin
  cursor past the last granted flame, retaining the next unserved source while
  full so early trees cannot monopolize slots during a short mass fire.
- Consume failed emission attempts: retain fractional credit only, not an
  ever-growing backlog that later produces an explosive burst.
- Track active/dropped counts for tests/profiling. Debug display is optional;
  a new product-facing control panel is not part of this task.
- Cull offscreen drawing, not simulation state or gameplay. Pool pressure and
  emission decisions must not depend on render FPS, window size or camera position.

At peak, roughly 8–10 live particles per fire lets hundreds of trees fit before
quality degrades. Capacity limits are intentional; not every extreme mass fire
must display every requested particle simultaneously.

## Intensity, emission and motion

Use normalized authoritative burn progress `u` from ignition/burnout ticks:

`intensity = smoothstep(0, 0.20, u) * (1 - smoothstep(0.75, 1, u))`

Clamp to 0–1; handle missing/invalid intervals by stopping emission. This gives a
short buildup, steady middle and a fade before burnout. It does not alter burn
speed, fuel, spread probability or the world's death deadline.

For each kind, accumulate `rate_per_sim_second * intensity * base_dt`, spawn the
integer part, and retain the fractional remainder. This avoids per-frame emission
and abrupt tick-sized batches. Seed independent visual samples from world seed,
emitter ID, kind and birth ordinal. Never seed by mutable pool slot or frame count.

Initial tuning for visual review, not gameplay balance:

| Kind | Peak rate/tree | Lifetime | Motion | Size/color |
| --- | --- | --- | --- | --- |
| Flame | 12/s | 0.35–0.7 s | Fast upward drift, slight randomized sideways motion | 1–2 art pixels; yellow → orange → red |
| Smoke | 0.75/s | 1–2.5 s | Slower rise and lateral drift | 1–2 pixels; muted gray with decreasing alpha |
| Ember | 1.5/s | 0.4–1.2 s | Initial upward/outward kick, then mild downward acceleration | 1 bright pixel; yellow/orange → dim red |

Spawn within the tree's lower/middle canopy, adjusting the source region for
saplings/dead trunks without relying on current screen dimensions. Use shared
world/art height conventions from tree rendering, not a zoom-dependent emitter.
Random initial velocities and lifetimes already provide variation; do not add
per-render-frame random jitter. Simple explicit integration and small per-kind
acceleration/drag are sufficient. There are no collisions or gameplay interactions.

Color/size evolution follows normalized particle age, using a few palette stages.
Smoke alpha can fade with age while its square silhouette stays sharp. Bright
solid ember pixels provide the glow impression; no bloom or blurred halos.
At burnout or emitter removal, stop new births immediately. Existing particles
finish their short lifetimes at their stored world positions/anchors; lingering
smoke is cosmetic and cannot reignite anything.

## Pixel rendering and depth order

Project world positions with the current MapView. Snap final positions to integer
physical framebuffer pixels. Square side length is an integer number of physical
pixels, using the same source-art pixel scale as tree sprites (minimum 1 pixel).
Convert back to SDL logical coordinates exactly once when render scale is active.
Keep squares physically square even if framebuffer X/Y scales differ.

Use untextured quads with flat vertex colors via SDL_RenderGeometry. No gradients,
texture filtering, rotation or antialiased circles. Standard alpha blending is
sufficient; save/restore renderer blend state and report drawing errors sensibly.

Respect the existing forest overlap behavior:

1. Build/reuse the ordered scene entries and particle index scratch buffers.
2. Group particles by stored emitter ground anchor, not by their rising Y position.
3. Merge emitter/tree entries by the same ground-depth convention as trees. Within
   an entry: smoke → tree sprite, if present → flames → embers.
4. Later/lower foreground trees cover effects from trees behind them. Do not draw
   every fire as one global overlay after the entire forest.
5. Render residual particles for removed emitters at their stored depth anchor.
   Draw ImGui afterward; effects never cover controls or alter picking masks.

Gather/bucket active particles once and reuse scratch storage. Do not rescan all
4,096 slots for each tree. Batch consecutive quads where blend/depth order allows;
preserve correct occlusion rather than forcing one global draw call. Use stable
birth order for within-emitter ties so pool compaction does not cause flicker.

## Implementation order and review gates

1. **Clock contract and emitter views**: implement the shared timing refinement in
   the foundation task and expose read-only fire views once fire exists. Test
   pause/reset/speed; no particles influence the world. Synthetic emitter fixtures
   may support isolated effects tests before the full fire tool is ready.
2. **CPU pool and emission**: required fields, fixed storage, kinds, seeded motion,
   intensity envelope, credits, safe removal and saturation policy. Headless CTest.
3. **SDL squares and layering**: crisp/DPI-aware geometry, merged tree/emitter depth,
   smoke/ember treatment and residual-particle handling. Visual review.
4. **Fire integration and validation**: connect real burners, regenerate/reset,
   pause/speed and many-fire load checks. Remove the superseded simple overlay.

Suggested short commits: `Add fire particle pool`, `Render pixel fire particles`,
`Connect particles to tree fires`. Request approval before each commit. Do not
start an implementation merely because this document exists.

## Validation

Add permanent CTest coverage, retaining all world snapshots unchanged:

- Required fields finite/initialized; lifetime positive; particles expire, including
  swapped-in entries; active count never exceeds capacity, even under overload.
- Stable seeded motion and emission for the same emitter/tick inputs; no dependence
  on pool slot, source-list order or render-frame partitioning for admitted time.
- Intensity builds, sustains and fades; zero intensity produces no emission. Credits
  behave for fractional rates and full pools, without deferred catch-up bursts.
- Pause freezes positions/age/color/credits/counters; speed scales all effects with
  the shared clock. A clamped 50x frame never exceeds 400 particle steps.
- Emitter disappearance stops births, allows bounded tails, and leaves no stale
  pointers. World reset clears everything, including credits and orphan emitters.
- Capacity/fairness are deterministic and bounded. World simulation replay is
  identical with particles disabled, tiny capacity and full capacity.
- SDL pixel checks verify sharp square footprints at 1x/1.25x/1.5x/2x density,
  correct alpha/blend restoration, depth occlusion and read-only render calls.

Manually review one fire, several clustered fires and hundreds of fires at 0x,
1x and 50x. Check buildup/burnout, occasional smoke rather than a solid cloud,
visible embers, paused window resizing, UI capture, foreground-tree occlusion,
and long-run stable memory/frame times. Profile before increasing complexity.

## Progress and handoff

- [x] Inspect renderer and foundation plan.
- [x] Define shared timing, particle data, pool, emission and draw order.
- [x] Foundation clock/fire prerequisites implemented.
- [x] CPU pool and tests implemented.
- [x] Renderer and depth integration implemented.
- [x] Real-fire integration implemented and headless regressions pass.
- [ ] Desktop visual/pacing review.

Implementation uses a separate GLM-only effects target with a fixed 4,096-slot
active prefix, fractional per-kind credits, independent keyed visual samples and
flame-first round-robin service. Saturation consumes failed attempts without backlog.
Emitter removal stops births while particles finish at stored ground anchors.

Application resets effects on world-generation changes and drives each admitted
base tick through world update, emitter refresh at world cadence, then particles.
Shared scene layout supports sprite picking and crisp physical-pixel squares.
TreeRenderer merges emitter/tree entries so smoke precedes the sprite and flames/
embers follow it, with later foreground trees covering complete background groups.

The debug build and thirteen CTest suites pass, including visual capacity-independent
gameplay replay, pool saturation/fairness, pause/reset, source/pool-order replay,
tick partitioning at 50x and transparent/DPI sprite picking. No image assets or
vendored files changed. Desktop inspection remains part of review; burned-ground
recovery is the foundation's next phase.

Permanent SDL memory-surface tests verify exact square coverage at fractional and
nonuniform DPI, clipped edges, blend restoration and read-only draws. A directed
rounding fix handles SDL's software quad/fill conversion without losing edge pixels.
One-off software checks passed alpha/layering, foreground occlusion and residual
particle depth. Real cluster/mass-fire captures at 2/6/12/24 seconds were inspected.

Synthetic Debug profiling of 400 peak ticks took approximately 95/125/444 ms for
500/1,000/6,912 emitters after caching intensity once per source/tick. Counts stayed
bounded at 4,096; overload dropped requests without backlog. Extreme all-cell fire
can cause noticeable Debug catch-up even though work remains bounded.

Mass-fire review strengthened fairness: the cursor advances with granted flames,
instead of moving only one source per tick. A sustained 3,000-fire regression
requires every source to receive visible flames during the middle of the burn;
a one-slot test prevents service from phase-locking onto a subset of equal fires.
