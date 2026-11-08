# Hex island generation

Branch: `feat/hex-island-generation`

## Current direction

The user rejected the initial sprite tileset. Start Milestone 1 again:
regular, unskewed hexagons created at runtime, with exactly one solid color
per tile. No terrain textures, outlines, shading, or sprite atlas.
**Trees and the castle are deferred to Milestone 2.**

The rejected generated sprites, contact sheet, metadata, and asset-generation
script were removed. The original `res/map/idea.gif` is preserved.

## Goals

- One connected, irregular island surrounded by water.
- Four terrain types: blue water, sandy coast, brown land, green grass.
- A seed reproduces the same map; new seeds change it.
- Keep generation independent of SDL rendering and ImGui controls.
- Preserve existing menus, Cozette font, and the non-docking UI.
- Use existing SDL3, ImGui, and GLM; no new dependencies.
- Ask for approval before each small, focused commit.

## Reference and visual decisions

Inspected the 30-frame GIF, including frame 0 and coalesced frames 9, 19, 29.
It is a muted, hard-edged cartographic illustration without a visible hex grid.
Retain its slate/olive/ochre palette, but the user's latest geometry and solid
fill requirements override the rejected flattened sprite design.

- Regular **pointy-top** hexes: width `sqrt(3)*radius`, height `2*radius`.
- Center basis: `(sqrt(3)*q + sqrt(3)*r/2, 1.5*r) * radius`.
- Integer lattice corner keys give adjacent cells identical shared vertices.
- SDL_RenderGeometry draws untextured flat triangles; no antialiasing or filtering.
- Map fits its available viewport without changing the hex aspect ratio.
- Water `#597187`, sand `#c5ad67`, land `#a8905b`, grass `#939f4b`.

## Implementation

- `hex_grid.h`: axial neighbors, centers, and regular hex corner lattice; GLM math.
- `tile_map.h`: terrain/seed/dimensions, rectangular odd-row storage with axial access.
- `island_generator.*`: seeded smooth value noise, radial falloff, largest connected
  land component, coastal sand, coherent interior grass/land patches.
- `map_renderer.*`: SDL geometry only; no generation or UI dependencies.
- `island_demo.*`: initial random seed, map state, Generate/Regenerate controls,
  editable seed and dimensions. Generate picks a new seed; Regenerate uses the
  entered seed. Width 12–96, height 12–72; default 48x32.
- `main.cpp`: draws the world before ImGui. Map framebuffer scale is applied once;
  ImGui's renderer handles its own framebuffer scaling.
- Root CMake uses the existing header-only GLM target. No CMake file in `src/`.

## Milestones and progress

| Milestone | Scope | State |
| --- | --- | --- |
| 1 | Procedural island, regular solid hexes, minimal seed/dimension controls | Complete; approved for commit |
| 1.5 | Await user instructions | Not started |
| 2 | Tree and castle design and deterministic object placement | Not started |
| Later | Camera interaction and other approved refinements | Not started |

Milestone 2 must keep objects separate from terrain: trees on eligible land,
one inland castle, no shared object occupancy. Review object visuals before
integrating them. Do not resurrect the rejected sprite assets.

## Validation

- Ninja debug build and `git diff --check` passed.
- Temporary independent C++ harness checked 500 maps across 100 seeds and five
  dimension combinations (including minimum, maximum, and narrow maps).
- Passed: seed determinism, 99/99 different-seed variations, nonempty connected
  island, water border, coast classification, axial/storage round trips.
- Passed: six regular corners at equal radius and two identical shared vertices
  for each adjacent hex pair; invalid dimension rejection and maximum uint32 seed.
- SDL software render of seed 74129 contains exactly four flat terrain colors
  plus the background, all opaque. Preview: `/tmp/miniworld-island-preview.png`.
- No application test framework exists; validation harnesses remain outside the
  repository. Real desktop UI interaction should also be checked by the user.

## Outstanding work / next action

- Milestone 1 is approved. Wait for Milestone 1.5 instructions; do not start
  Milestone 2 or further refinements yet.
- No trees, castle, object data, camera, grid overlay, or gameplay is implemented.
- Same-color adjacent hexes intentionally merge visually: no grid outlines.
- Rendering currently builds a small vertex batch each frame; caching can wait
  until measurement justifies it. UI reserves space on the left for controls.
- Keep unrelated edits to AGENTS.md and the staged `.run/Miniworld.run.xml`
  deletion untouched. Do not commit unrelated files.

## Future improvements (not implemented)

Trees/castle (Milestone 2), camera pan/zoom, optional grid and tile selection,
terrain variants, multiple islands, animated water, mountains, roads, villages,
biomes, pathfinding, and world persistence. No gameplay or simulation yet.
