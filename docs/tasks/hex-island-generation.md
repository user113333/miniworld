# Hex island generation — completed

All milestones were approved, committed and merged into `master`. The local
`feat/hex-island-generation` and `feat/imgui-font-docking` branches were deleted.
Current behavior and review follow-up are in [project status](../project-status.md).

## Milestone record

| Milestone | Delivered | Commit |
| --- | --- | --- |
| 1 | Procedural island, solid regular hexes, seed/dimension controls | `afda00d` |
| 1.5 | Full water background, centered world origin, Island window toggle | `13ec2f7` |
| 2 | User tree sprites, deterministic placement, density control | `5781bd7` |
| 3 (final) | Smaller clustered forests, overlapping trees, depth sorting | `2463c71` |

The run-configuration deletion was committed separately as `289a330` before the
fast-forward merge into master. No further milestones are planned.

## Final design

The original GIF inspired the slate/olive/ochre palette. The user rejected the
first generated tileset and requested regular, unskewed runtime hexagons with
one solid color per tile. That rejected tileset and its tool were removed.
The original GIF is no longer present.

- Pointy-top hexes: width `sqrt(3)*radius`, height `2*radius`.
- Axial center basis: `(sqrt(3)*q + sqrt(3)*r/2, 1.5*r) * radius`.
- Integer corner lattice gives neighboring tiles identical shared vertices.
- Rectangular odd-row storage; world origin is the middle tile and screen center.
- Water `#597187`, sand `#c5ad67`, land `#a8905b`, grass `#939f4b`.
- Seeded smooth noise plus radial falloff; retain the connected island containing
  `(0, 0)`, derive sand from coastal neighbors, and use coherent interior terrain.
- Dimensions 12–96 by 12–72; default 48x32. Default tree density 20%.
- User tree1/2/3 PNGs are unchanged transparent 8x8 sprites, separate from terrain.
- Forests rank eligible grass/land cells using smooth seeded noise with slight
  local variation. Density selects that fraction, preserving existing trees as it grows.
- Seeded ground offsets stay inside each owning hex (maximum 0.4 radii per axis).
- Tree target width 1.05 tile widths, rounded down to integer physical-pixel scale.
  Bottom-center anchors snap to framebuffer pixels. Nearest filtering is tree-only.
- Canopies may overlap. Sort by actual snapped ground Y, then X and variant for ties.
- Draw terrain, then trees, then ImGui. Keep generation independent of SDL/ImGui.
- Cozette, the non-docking UI, regular solid terrain, and centered origin are retained.

Castle work was cancelled. Houses, camera movement, grid overlays, gameplay and
further world-generation features require a separate user request.

## Historical validation

One-off C++ harnesses checked:

- 900 maps across odd/even and narrow dimensions: round trips, centered projection,
  regular/shared hex corners, connected inland origin, water borders, deterministic seeds.
- 250 maps for tree eligibility, unique occupancy, density endpoints, unchanged terrain,
  all sprite variants and stable offsets. At 20% density across 50 standard maps,
  4,151 of 4,318 trees had at least two tree neighbors (96%).
- Exact sprite colors at 1x, 1.25x, 1.5x and 2x framebuffer scales; reversed input
  tree order produced identical output. In a same-row overlap fixture, all 101
  overlapping pixels matched the lower tree's isolated render.
- Application startup, origin placement and Island window close/reopen behavior.

Ninja builds passed throughout. These historical harnesses were not committed;
see project status for current validation, manual checks and the durable-test backlog.
