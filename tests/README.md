# Regression tests

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The twenty-one suites cover hex geometry/storage, island invariants, forest placement,
fixed-seed snapshots, simulation clock accounting and controller pause/reset
behavior, persistent forests, fire/recovery, CPU particles, sprite picking, shared replay
and Flecs ownership/identity checks, plus entity inspection/editing and a
production GUI explorer smoke test, content catalog, shared texture cache and
production Assets browser checks.
Checks stay active in Release. Domain/effects tests need no SDL or display. The
renderer suites draw to SDL memory surfaces. The GUI explorer suite uses the
SDL dummy video driver and a software renderer; no desktop display is needed. No new test library is required.
Use `-DBUILD_TESTING=OFF` when configuring to omit the test executables.
The `release` configure/build/test presets run the same checks with optimization.

## Seed snapshots

`fixtures/` records the current approved generation output:

| Seed | Size | Density |
| --- | --- | --- |
| 0 | 12x12 | 20% |
| 74129 | 24x18 | 20% |
| 4294967295 | 25x19 | 35% |

Terrain rows use storage order: `.` water, `s` sand, `l` land, `g` grass.
Each tree record is `q r variant offset_x offset_y`, with offsets in hex-radius
units. Comparisons ignore tree storage order and allow 0.000001 offset rounding.

Keep snapshots unchanged during refactors. Update them only for an intentional,
reviewed generation change; they are never regenerated automatically by tests.
Permanent UI-event and full-scene depth coverage remains future work; those checks
currently use separate headless smoke harnesses.

## Flecs runtime checks

`world.flecs_world` directly checks component ownership, actor factories, query
results after table changes, read-only value views, deletion/reuse, old-world
handles and fixed-clock pause/reset behavior.

`fixtures/flecs_runtime.txt` contains 24 checkpoints updated for the simplified
tree lifecycle. Rows are `seed tick digest`. Each replay
uses a 24x18 island at 100% initial density, ignites the first/last original
placement entries at tick zero and queues the middle entry before tick 128.
Checkpoints include terrain, recovery and every tree fire/remains field through
400 seconds. The digest is FNV-1a over eight little-endian bytes per field, with
float bit patterns encoded as unsigned integers and actors sorted by gameplay ID.
Keep these checkpoints unchanged during refactors.

`test_state.h` contains editable inputs and value snapshots. `world_fixture.h`
applies fixture edits to real ECS components before running the existing behavior
assertions through Flecs systems. Its vectors are test data, never runtime actor
storage. The direct Flecs suite separately checks handles and authoritative state.

## Entity Explorer

`world.entity_explorer` checks all registry categories, generic reflection,
relationships, read-only policies, atomic edits, conflicts, geometry/occupancy,
Flecs notifications, scene/picking refresh, paused/running commands and stale IDs.
`gui.entity_explorer` drives the production ApplicationUi with a 5,000-entity
list, staged-form protection, Apply/Revert, paused ignition, mouse capture and
regeneration. Set `EXPLORER_SCREENSHOT` to a BMP path when running this executable
to capture rendered GUI frames for review.

## Assets

`content.catalog` checks deterministic normalized keys, metadata/source formats,
font bytes, explicit roots and load diagnostics. `render.texture_cache` uses a
software renderer to check texture reuse, dimensions/source alpha, nearest
sampling, tint restoration, shared actor bindings and partial startup cleanup.
`gui.asset_browser` uses the production UI with a temporary resource root and
1,000 extra files. It covers navigation, search/filter/sort, keyboard selection,
thumbnail size, clipping, hidden selection, input capture, required font failures,
optional preview errors, reopening and regeneration retaining cached content.
Set `ASSET_SCREENSHOT` to a path prefix to save review BMPs, including a synthetic
1.5× DPI layout. The GUI suite uses SDL's dummy driver without a desktop display.

Root CMake stages all development resources, including sources/licenses. An
add/remove PNG build check confirms no filename list or stale staging is needed;
Debug/Release application smoke launches from `/tmp` confirm working-directory
independence. Native monitor/DPI transitions still require desktop review.
