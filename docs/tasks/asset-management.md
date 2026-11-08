# Asset management and browser

Status: planned; no implementation yet.

## Goal

Create a small content manager before the game gains more actor sprites and
other assets. Centralize discovery, identification and loading, and make the
actual game assets explorable in an application-owned ImGui Assets window.

Use the grid layout and interactions in ImGui's Assets Browser example as a
reference, rather than displaying the demo's generated items. The local example
is `ExampleAssetsBrowser` / `ShowExampleAppAssetsBrowser()` in
`lib/imgui/imgui_demo.cpp`. Leave vendored ImGui unchanged.

## Current integration points

- `ActorRenderer` resolves `res/map/` paths and owns five tree textures plus
  their source alpha masks. Future settlers, houses and ships need more assets.
- `ApplicationUi` independently resolves and loads the GUI font into its atlas.
- Root CMake currently copies only the font/license and five tree images into
  the build output. Other files under `res/` are not all available at runtime.
- `res/` already includes house/ship PNGs and `.aseprite` source files. A browser
  must distinguish discoverable content from formats supported for runtime use.
- The existing top bar has File and View menus. Asset browsing belongs beside
  Island and Simulation, independent of whether the ImGui demo is open.

## Content manager

- Propose `src/content/` for the catalog, asset identifiers, metadata and resource
  root/path resolution. Keep it free of ImGui and separate from the Flecs actor
  store; assets are shared content, not simulation actors.
- Let Application own the manager and pass explicit access to consumers. Avoid
  a global singleton or each window building an independent asset catalog.
- Use normalized paths relative to `res/` as stable asset keys, independent of
  discovery/sort order. Record type, name, relative path, file size, supported
  loader, load status and diagnostics. Image dimensions become available after
  decoding. Folder entries support navigation rather than becoming loadable assets.
- Resolve the runtime resource root once from the application/executable
  location, with an explicit root supplied by tests. Do not depend on the
  process working directory or silently fall back to a source checkout.
- Discover files recursively and keep catalog order deterministic. Adding a
  supported image should make it discoverable without a new hard-coded filename
  list in the browser. Named gameplay bindings may still select specific assets.
- Initially support PNG images and access to font file data. Keep `.aseprite`,
  licenses and other unsupported files visible with type/metadata and a clear
  source-only or unsupported status. Native Aseprite decoding/export and animation
  playback are separate work, not implied by listing the source files.
- Validate missing, duplicate or invalid asset keys and report load failures
  with the asset path. Required gameplay/font failures retain explicit startup
  errors; one bad optional preview must not close the browser or application.

## Resource ownership and consumers

- Keep SDL decoding/textures and their cache in `src/render/`. Share each loaded
  texture between world rendering and browser previews instead of uploading
  duplicate textures. Catalog records and handles must not own SDL resources.
- Decode each cached PNG once for texture creation and source alpha data. Keep
  picking masks/dimensions consistent with the image that is actually drawn.
- Replace ActorRenderer's direct file loading with content lookups, preserving
  sprite variants, smooth camera sizing, nearest sampling, alpha picking and
  fire tint. Do not tie actor appearance to catalog display order.
- Keep font configuration and the font atlas in `src/gui/`; use the manager for
  font identification/data access without moving ImGui lifecycle out of that
  layer. Respect font byte ownership for as long as the atlas needs the data.
- Define lifetimes explicitly: detach consumers and finish GUI draws before
  freeing cached textures; release SDL textures before destroying the renderer.
  Partial initialization failures must follow the same ownership rules.
- Browsing must not alter shared texture tint, alpha or sampling state used by
  the scene. Preview original artwork rather than a tree's current burning tint.
- World regeneration retains shared assets; it resets actors, not the content
  manager. Browsing and loading presentation content must not change simulation
  state, randomness or time.

## Assets window

- Add a direct **Assets** button to the existing top bar and a checked
  **View → Assets** menu item. Both open/focus the same closable window; closing
  it must leave it reopenable. No dependency on the ImGui demo window.
- Keep the window and its state in `src/gui/asset_browser.*`, called through
  `ApplicationUi`. Keep searches, sorting and selection out of Application.
- Show folders/navigation and a responsive thumbnail grid with readable names.
  Provide search by name/path, asset-type filtering and name/type sorting.
- Follow the demo's useful interactions: selection, keyboard navigation and
  adjustable thumbnail size. Preserve selection by asset key when sorting or
  filtering, with defined behavior when the selected asset is hidden.
- Show PNG thumbnails over a transparency background, preserving aspect ratio
  and crisp pixel art. Unsupported formats get a type icon and explanatory
  status instead of an empty or misleading image.
- Selecting an item shows a larger image preview where supported, plus its
  relative path, type, dimensions, file size and load/error status. Non-image
  files still expose metadata; font preview is optional later work.
- List assets that are not currently used in the scene, including house/ship
  PNGs. Distinguish not-yet-loaded previews from failed or unsupported assets.
- Scan/catalog once at startup and load previews on demand through the shared
  cache. Use clipped grid rows; avoid rescanning directories or decoding every
  file on each frame. Background loading and cache eviction can follow if needed.
- Respect existing GUI input capture: typing in search, navigating items and
  changing thumbnail size must not trigger world tools or camera zoom/panning.

## Packaging and scope

Define runtime resource staging in the root `CMakeLists.txt`. Use one resource
layout for both presets and stage all browser-visible content, including
source-only files and license/attribution documents for this first development
version. Ensure additions/removals are reflected after the normal configure/build
workflow, without leaving stale browser entries in the staged directory.

Do not edit supplied artwork, vendored libraries or simulation behavior.
The first browser is read-only: importing, renaming/deleting files, drag-and-drop
placement in the world, hot reload, asset editing, atlases and a production
asset-conversion pipeline are later tasks. Do not implement loaders for hypothetical
future asset types yet.

## Implementation milestones

1. **Catalog and staging:** stable keys, root resolution, metadata, type detection,
   diagnostics and complete development resource staging.
2. **Shared loading:** renderer texture/mask cache and migration of tree/font
   consumers, preserving appearance, picking and startup/shutdown behavior.
3. **Browser:** top-bar/menu access, folder exploration, searchable/sortable grid,
   selection, thumbnails and details using real catalog entries.
4. **Integration:** input capture, DPI/layout, optional-load failures, clean
   shutdown and validation; update project status with the resulting architecture.

## Acceptance and validation

- Launch from a working directory outside the repository. Assets still resolve
  from the staged resource root in Debug and Release.
- A newly added PNG appears after the normal configure/build workflow without
  editing a browser filename list. Removed resources do not remain as stale files.
- Browse current tree, house and ship images, the font and source-only Aseprite
  files. Supported images preview; unsupported files remain identifiable.
- Opening/closing/reopening through the top bar and View menu uses one window.
  Search/filter/sort, selection, keyboard navigation and thumbnail resizing work
  on both short and scrollable catalogs.
- Repeated browsing reuses loaded textures/masks. Tree rendering and alpha
  picking remain correct; previews do not inherit/leak fire tint.
- Missing/corrupt optional previews show actionable errors. Required asset
  failures and partial startup release resources safely before SDL teardown.
- Domain/effects code remains independent of SDL and ImGui. Add focused catalog
  tests and software-renderer checks for cache reuse, mask consistency and
  lifecycle. Run root Debug/Release builds and CTest presets; visually review
  thumbnails, navigation, DPI and GUI/world input capture.
