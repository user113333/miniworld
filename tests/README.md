# World regression tests

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The seven suites cover hex geometry/storage, island invariants, forest placement,
fixed-seed snapshots, simulation clock accounting and controller pause/reset
behavior, and living-forest lifecycle, occupancy and seeded replay. Checks stay
active in Release. Tests link only the world logic and GLM; they need no display,
SDL initialization, or new test library.
Use `-DBUILD_TESTING=OFF` when configuring to omit the test executables.

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
UI, rendering, DPI and pixel-level depth tests remain separate future work.
