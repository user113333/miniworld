# Hex island generation

Branch: `feat/hex-island-generation`

## Current direction

The user rejected the initial sprite tileset. Start Milestone 1 again:
regular, unskewed hexagons created at runtime, with exactly one solid color
per tile. No terrain textures, outlines, shading, or sprite atlas.
**Milestone 2 adds trees using the user's sprites. Castle is Milestone 3.**

The rejected generated sprites, contact sheet, metadata, and asset-generation
script were removed. The original reference GIF was inspected during initial
work; it is no longer present. Current map assets are the user's three tree PNGs.

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
- `tile_map.h`: terrain/seed/dimensions, rectangular odd-row storage with axial
  access centered on the middle tile. World coordinates extend negatively and
  positively around `(0, 0)`; storage indices remain nonnegative.
- `island_generator.*`: seeded smooth value noise, radial falloff centered on
  `(0, 0)`, connected land component containing the origin, coastal sand,
  coherent interior grass/land patches.
- `map_renderer.*`: SDL geometry, shared terrain colors and map-view fitting;
  no generation or UI dependencies.
- `island_demo.*`: game state, settings and Generate/Regenerate actions only.
- `tree_placement.*`: independent, seeded tree occupancy and sprite variants;
  terrain data is never modified by object placement.
- `tree_renderer.*`: loads user PNGs, nearest texture filtering, integer physical
  pixel scaling, bottom-center ground anchors, and back-to-front draw ordering.
- `island_controls.*`: ImGui seed and dimension controls. Generate picks a new
  seed; Regenerate uses the entered seed. Width 12–96, height 12–72;
  default 48x32. ApplicationUi owns visibility via View > Island and the
  window's close button. Tree density updates the current world's trees live;
  default 20%, range 0–100%, with a visible tree count.
- `main.cpp`: draws the world before ImGui. Map framebuffer scale is applied once;
  ImGui's renderer handles its own framebuffer scaling.
- Root CMake uses the existing header-only GLM target. No CMake file in `src/`.

## Milestones and progress

| Milestone | Scope | State |
| --- | --- | --- |
| 1 | Procedural island, regular solid hexes, minimal seed/dimension controls | Committed: afda00d |
| 1.5 | Full water background, centered island origin, toolbar window toggle | Committed: 13ec2f7 |
| 2 | User tree sprites, deterministic placement and density control | Complete; approved for commit |
| 3 | Castle design and placement | Not started |
| Later | Camera interaction and other approved refinements | Not started |

Milestone 2 uses `res/map/tree1.png`, `tree2.png`, and `tree3.png` unchanged.
All are transparent 8x8 PNGs. CMake copies them alongside the executable.
At most one tree per grass/land tile; no trees on water or sand. A coordinate
hash of the world seed determines both placement and variant, so density changes
retain existing trees and do not reroll the landscape. SDL_LoadPNG needs no new
dependency. Terrain is drawn first, then trees, then ImGui.

Milestone 3 will add exactly one inland castle without tree occupancy conflicts.
Review its visual design first; do not resurrect the rejected castle sprite.

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

Milestone 1.5:

- Water color clears the entire framebuffer, blending with map water outside
  the finite generated grid. No new water tiles or generation changes.
- Map is centered on the full window with symmetric margins and no reserved
  sidebar. Island controls and menu overlay it; hiding the controls does not
  change the map transform.
- Cross and `(0, 0)` label mark the island's axial origin at the exact screen
  center. Generation uses that origin as its radial center and retains the
  connected island containing it. The marker shares the renderer's MapView.
- Default control-window position is upper right; saved ImGui positions persist.
- Passed: Ninja build, viewport corner water-color checks, simulated mouse
  close/reopen through View > Island, and unchanged map data after toggling.
- Passed: 900-map checks across odd/even and narrow dimensions, inland origin,
  centered projection, coordinate round trips, connectivity, and all tile
  corners fitting the viewport. Determinism and water borders remain valid.
- Visually inspected SDL-rendered preview: `/tmp/miniworld-milestone15-preview.png`.

Milestone 2:

- Passed: Ninja build and independent checks across 250 maps, 50 seeds and five
  dimension combinations. Deterministic placement and variants, unique eligible
  tile occupancy, unchanged terrain, zero/full density endpoints, and all three
  sprite variants checked.
- Passed: repeat regeneration, live tree-density update without terrain changes,
  reverse-input draw ordering producing identical output, exact sprite colors
  without smoothing at 1x/1.25x/1.5x/2x framebuffer scales, and Island
  close/reopen UI checks.
- Tree anchors snap to framebuffer pixels. Nearest filtering is set only on the
  three tree textures; ImGui filtering is unchanged. Pixel scale is integral,
  including on high-DPI framebuffers.
- Preview: `/tmp/miniworld-milestone2-preview.png`. User sprites remain untouched.
- Real desktop appearance and density-slider interaction still need user review.

## Outstanding work / next action

- Milestone 2 is approved. Next: review castle visuals for Milestone 3, then
  add exactly one seeded inland castle without tree overlap. Await instructions
  before starting.
- No castle, camera, grid overlay, or gameplay is implemented.
- Same-color adjacent hexes intentionally merge visually: no grid outlines.
- Rendering currently builds a small vertex batch each frame; caching can wait
  until measurement justifies it. UI windows overlay the world.
- Keep unrelated edits to AGENTS.md and the staged `.run/Miniworld.run.xml`
  deletion untouched. Do not commit unrelated files.

## Future improvements (not implemented)

Castle (Milestone 3), camera pan/zoom, optional grid and tile selection,
terrain variants, multiple islands, animated water, mountains, roads, villages,
biomes, pathfinding, and world persistence. No gameplay or simulation yet.
