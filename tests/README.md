# Regression tests

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The fifteen suites cover hex geometry/storage, island invariants, forest placement,
fixed-seed snapshots, simulation clock accounting and controller pause/reset
behavior, living forests, fire/recovery, CPU particles, sprite picking and shared replay.
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
