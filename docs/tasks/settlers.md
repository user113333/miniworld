# Settlers

Status: **all three phases implemented**.

Implement **only** the scope below, in order: **1. logic, 2. rendering, 3. ImGui**. Complete and test one phase before starting the next. Do not introduce houses, ships, reproduction, food, or other features from the old settlers plan.

## 1. Simulation logic (first)

### Settlers and movement

- Generate **3 settlers** at random, valid, separate positions on the current island. Each begins with **500/1000 happiness** and an **empty inventory**.
- Use continuous world-space positions: settlers move freely within and between hexes, **not in discrete tile-sized steps**.
- Movement collision depends **only on terrain type**. Water is impassable; mountains will also be impassable when mountain terrain is added later. Settlers may pass through other settlers, trees, and other actors; do not add actor collision or avoidance.
- Limit wandering and the search for cutting/planting targets to **10 hexes** from the settler's current position.
- Use a **small, bounded A* search on walkable hexes** for navigation, then follow its waypoints using smooth world-space movement. Do not allow a movement segment to cross blocked terrain. Reuse a route until it becomes invalid; do not pathfind every frame. No navmesh, physics engine, or expensive crowd steering.
- Each settler's speed is a DNA trait, from **0.5x to 1.5x** the shared base speed.

### Simple DNA and decision-making

- Give every settler a small, seeded set of fixed DNA traits: **movement speed, activity preferences, and sensitivity to nearby settlers/trees**. More complex genetics are deferred.
- Use a **lightweight weighted-random decision selector plus a small action state machine**, not a planner, behavior-tree framework, or per-frame AI.
- Available actions: **idle, wander, cut a tree, plant a tree**. Settlers choose for themselves according to their DNA and available targets/resources; there is no forced cutting/planting cycle.
- Settlers should spend **approximately 80% of simulation time idling**. Measure this by elapsed activity duration, not just the fraction of decisions. Keep inactive settlers cheap to simulate.
- A settler normally finishes its chosen valid action before choosing another. If a target becomes invalid or unreachable, stop safely and choose again after a bounded delay.
- Happiness does **not** affect action choice in this version; unhappy fire-setting and island travel are deferred.

### Inventory, cutting and planting

- Personal inventory: **8 slots**, each holding **one item type** and up to **10 units** of that type. Only two item types: **seeds** and **logs**. No shared storage or trading.
- Cutting a fully grown tree takes **3 seconds** and removes that tree, awarding **2 seeds and 4 logs**. Put in as much as fits; **discard excess** without stopping the cut. Never award the yield twice.
- Planting takes **2 seconds**, consumes **1 seed**, and places a **growing tree** on any empty, valid grass or land tile in range. Do not add spacing rules between trees.
- A planted tree becomes fully grown after **60 seconds of simulation time**; only fully grown trees can be cut.
- Preserve existing forest behavior: **initially generated trees begin fully grown**, trees do **not** reproduce or regrow naturally, and existing fire/charred-terrain behavior remains intact. Growth applies only to planted trees.
- Ensure two settlers cannot harvest the same tree twice or plant on the same occupied tile. Cancel invalid work cleanly, including when fire removes the target.

### Happiness and thoughts

- Happiness is an integer **0–1000**. It starts at **500**, is capped at **1000**, and when it reaches **0**, the settler is **removed immediately** with its entire inventory (no drops or corpse).
- Each settler has an occasional **thought every 10–30 simulation seconds**; this is not a continuous happiness adjustment as the settler moves.
- On an environmental thought, examine nearby settlers and trees within a **5-hex radius**. Comfortable surroundings restore happiness (initial tuning: **+10**); overcrowding lowers it. **Nearby settlers must penalize happiness more strongly than trees**, with severity modified by the settler's DNA sensitivity.
- Apply the entire happiness change **immediately when the thought occurs**. Keep thresholds and penalty values simple and configurable in simulation tuning; do not build a complex mood system.
- Thoughts also announce activity decisions (e.g. "I'll rest", "Time to plant", "I'll cut that tree") as well as happiness (e.g. "Too many people here", "I like it here"). Store the **current thought text and its remaining display time** in simulation state, independent of rendering.
- Every new thought is visible for **10 seconds of simulation time**, then disappears. A newer thought can replace an older one.

### Performance and architecture

- Integrate with the existing **Flecs** actor registry, GLM positions, deterministic world generation, versioned actor handles, occupancy rules, seeded randomness, and the shared fixed-step clock described in `project-status.md`.
- Keep movement on the existing **32 Hz** clock; evaluate higher-level actions only when their timer/job requires it, and surroundings only at the scheduled thought times. Pause, speed, and regeneration must affect settlers consistently.
- Trees use static tile occupancy; settlers are moving actors and must **not** overwrite tree occupancy. Their movement does not block a tile for other settlers.
- Put actor data in `src/actors/`, simulation systems in `src/simulation/`, terrain/hex helpers in `src/world/`; keep simulation independent of SDL and ImGui.
- Add focused deterministic tests for navigation, idle-heavy AI, DNA bounds, inventory overflow, exclusive tree cutting, planted-tree growth, timed thoughts, happiness/death, and regeneration. Run the existing Debug/Release build and CTest presets.

## 2. Rendering (second)

**STOP AND REMIND THE USER:** *You said you will integrate the `.aseprite` reading library yourself before the rendering phase.* Do not choose, implement, or vendor an Aseprite loader on the user's behalf.

- After that integration, render settlers moving smoothly using the project's existing scene/camera conventions.
- Draw a small, simple **thought bubble above each settler's head** when they have a current thought; Codex designs its appearance.
- Use **windowless ImGui drawing** (draw-list overlay, not an ImGui window) for the bubble. It must follow the settler on screen and disappear after 10 simulation seconds.
- Keep logic independent of visuals. Do not add extra effects or gameplay.

## 3. ImGui interaction (third)

- Make settlers **clickable/selectable in the scene like trees**, reusing the existing picking/selection and Entity Explorer architecture.
- Create a **Settlers ImGui window** to inspect the selected settler: happiness, DNA traits, current action/thought, and inventory.
- Show all **8 inventory slots** and allow the user to change item type/quantity. Validate slot limits (**0–10**) and apply changes through the existing safe, queued edit-command approach; handle deleted settlers/regeneration safely.
- Add a **spawn-settler tool**: the user activates it in ImGui and **clicks on a valid land location** to spawn a settler there. Water and future mountain locations are invalid. New settlers receive random DNA, 500 happiness, and an empty inventory.
- Do not add reproduction or automatic population replacement.

## Deferred tasks (do not implement now)

- [Ships](ships.md)
- [Multiple islands](multiple-islands.md)
- [Settler fire behavior](settler-fire.md)
- [Houses](houses.md)
- [Mountains](mountains.md)
- [Reproduction and genetics](reproduction-genetics.md)

## Done means

1. **Logic:** Three initial settlers behave autonomously, spend most of their time idle, move without tile-step snapping, cut/plant trees, carry resources, think, gain/lose happiness, and die correctly; existing forest/fire behavior still passes tests.
2. **Rendering:** After the user's Aseprite-library integration, settlers and 10-second thought bubbles are visible without putting GUI dependencies into simulation.
3. **ImGui:** A settler is selectable, its inventory is editable, and a settler can be spawned by clicking valid terrain.

**Do not begin a later phase or any deferred task without the user's approval.**
