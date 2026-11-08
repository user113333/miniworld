# Regression tests

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The sixteen suites cover hex geometry/storage, island invariants, forest placement,
fixed-seed snapshots, simulation clock accounting and controller pause/reset
behavior, living forests, fire/recovery, CPU particles, sprite picking, shared replay
and Flecs ownership/identity checks.
Checks stay active in Release. Domain/effects tests need no SDL or display. The
renderer suites draw to SDL memory surfaces without a window or display
driver. No new test library is required.
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

## Flecs migration checks

`world.flecs_world` directly checks component ownership, actor factories, query
results after table changes, read-only value views, deletion/reuse, old-world
handles and fixed-clock pause/reset behavior.

`fixtures/flecs_runtime.txt` contains 24 checkpoints captured from the pre-Flecs
implementation, before the migration. Rows are `seed tick digest`. Each replay
uses a 24x18 island at 100% initial density, ignites the first/last original
placement entries at tick zero and queues the middle entry before tick 128.
Checkpoints include terrain, recovery and every tree lifecycle field through
400 seconds. The digest is FNV-1a over eight little-endian bytes per field, with
float bit patterns encoded as unsigned integers and actors sorted by gameplay ID.
Keep these checkpoints unchanged during refactors.

`test_state.h` contains editable inputs and value snapshots. `world_fixture.h`
applies fixture edits to real ECS components before running the existing behavior
assertions through Flecs systems. Its vectors are test data, never runtime actor
storage. The direct Flecs suite separately checks handles and authoritative state.
