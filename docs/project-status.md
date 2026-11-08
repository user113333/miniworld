# Project status

The hex-island project is complete and merged into `master`. Both local feature
branches were deleted. Read `AGENTS.md` and check `git status` before working.
The milestone record is [here](tasks/hex-island-generation.md).

## Approved behavior

- Regular pointy-top hexes drawn at runtime, one solid color per terrain tile.
- One connected seeded island: water, coastal sand, brown land, green grass.
- World `(0, 0)` is an inland tile at the exact screen center. Water fills the
  whole background; the finite map border is invisible against it.
- User-supplied `res/map/tree1.png` through `tree3.png`, unchanged 8x8 sprites.
  Small, overlapping forest groups; trees with lower ground positions draw last.
- Seeded forest ranking, variants and offsets. Density is the occupied fraction
  of eligible grass/land tiles; increasing it grows existing forest patches.
- Nearest tree filtering, integer physical-pixel sprite scaling, snapped anchors.
- Cozette at 13 px with FreeType monochrome hinting; ImGui docking is removed.
- View > Island and View > ImGui demo toggle the windows. Island can be closed.
- Generate chooses a random seed; Regenerate uses the edited seed/dimensions.
  Density updates the current world's trees without changing terrain.
- No castle, house integration, camera, grid, or gameplay. `house1.png` is unused.

## Code ownership

| Files | Responsibility |
| --- | --- |
| `main.cpp` | SDL/ImGui lifecycle and frame orchestration |
| `application_ui.*`, `island_controls.*` | Menus, controls, font/DPI, origin hint |
| `island_demo.*` | World state, settings and generation actions; no UI/rendering |
| `hex_grid.h`, `tile_map.h` | Axial geometry and centered odd-row storage |
| `island_generator.*`, `tree_placement.*` | Seeded terrain and forest generation |
| `map_renderer.*`, `tree_renderer.*` | SDL drawing, map fitting and sprite resources |

Use the existing SDL3, ImGui, GLM submodules and system FreeType. SDL includes
PNG loading; SDL_image is unnecessary. Root CMake copies required assets next
to the executable. Keep CMake in the root and use the Ninja `debug` preset.

## UI fixes after project review

Implemented and validated:

1. UI actions run before `fit_map`. A newly generated map uses its own dimensions
   on the same frame. `draw_origin_hint` uses the resulting MapView separately.
2. Island's first-open position uses viewport coordinates, current toolbar height
   and DPI-scaled margins/initial width. Saved ImGui positions remain honored.
   Current frame height is checked because viewport WorkPos can lag by a frame.

Frame order: platform/backend NewFrame → UI actions → fit current map → origin
hint → ImGui::Render → water clear → terrain → trees → ImGui → present.
Map drawing applies framebuffer scale once; ImGui's backend handles its own scale.

## Validation and reproduction

```sh
cmake --preset debug
cmake --build --preset debug
./cmake-build-debug/miniworld
```

Passed: Ninja debug build and `git diff --check`. Targeted headless SDL software
checks executed the production frame code with simulated mouse input:

- Click Regenerate after changing 48x32 to 96x72, then to 12x12. All tile corners
  fit the viewport immediately on each button-release frame; origin stays centered.
- Fresh Island placement at 100%, 150%, 200%, 300% simulated content scale stays
  below the toolbar. At 300%, its top is 93 px and toolbar bottom is 57 px.
- A saved Island position `(150, 180)` remains unchanged at all tested scales.

For desktop review, repeat dimension changes and check first-open placement with
fresh ImGui settings, then verify an existing saved position is not reset. Native
monitor transitions still need manual checking; simulated scale is not a substitute.

There is no committed test runner yet. Earlier generation/rendering checks used
one-off harnesses outside the repository; temporary files and screenshots are not
required project artifacts and should not be relied on after a session reset.

## Deferred review suggestions

These are future work, not authorization to start another milestone:

1. Add small repository-owned CTest regressions for coordinates, seed fixtures,
   connected island/water borders, tree eligibility/density/depth, and UI behavior.
2. Simplify SDL/ImGui resource ownership and repeated cleanup in `main.cpp`.
3. Improve names (`IslandDemo`, `storage_origin`, `Tree::offset`), centralize map
   limits and name generation/forest tuning values. Preserve current seeded output.
4. Consolidate duplicated hash/noise code only with seed-regression protection.
5. Enable warnings on application targets. The review's `-Wall -Wextra -Wpedantic`
   check found a missing `TileMap::trees` initializer warning in the generator;
   the vector is correctly default-initialized, but explicit intent would be clearer.
6. Report SDL draw-call failures. Profile before adding render caches or abstractions.

No source changes for these deferred suggestions were made as part of the two UI fixes.
Follow AGENTS.md: summarize validation and wait for approval before every commit.
