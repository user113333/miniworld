# Settlers, construction and sailing

Status: draft plan; no gameplay changes implemented.

Confirmed name: **settlers**.

## Goal and scope

Introduce autonomous people who gather resources, establish a home and can build
and travel aboard a ship. Complete this as small milestones, each playable and
validated before starting the next.

- Settlers choose their own actions using simulation-owned AI.
- Each settler has an inventory and can cut down trees to obtain wood.
- Having enough food is a prerequisite for building a house. Wood supplies the
  building material; the food source and consumption rule need agreement below.
- Houses can catch fire and burn down, including fire spreading from trees.
- Settlers move continuously through world space using pathfinding. They are
  not restricted to tile centers or movement in discrete tile-sized steps.
- Add mountain terrain that settlers cannot walk across.
- Settlers can build a ship, board it, travel over water and disembark on land.
- Support sailing around the current island. Multiple islands are future work.
- Hunger and health are future features. Do not add hunger meters, food decay,
  starvation, health points or settler damage/death in this task.

Tree lifecycle simplification is complete. Removing a healthy tree through
harvesting extends the current fire-only death rule. Do not restore natural death or
automatic tree regrowth; settler planting remains future work.

## Proposed first version

These choices keep the first implementation small; they are proposals rather
than additional approved gameplay requirements.

### Inventories and work

- Start with wood and food as integer item quantities in each settler's personal
  inventory. No shared global stock, trading or storage buildings yet.
- Configure starting population, inventory limits, recipes, work durations and
  yields in simulation tuning. Every recipe must fit in a worker's inventory.
- Cutting takes simulation time and requires being within reach of a healthy,
  non-burning tree. Remove it and award wood exactly once on completion. Do not
  turn harvesting into fire damage or leave a charred tile.
- Reserve the target while working so two settlers cannot harvest the same tree
  or build over the same site. Release reservations when work ends or fails.
- Validate the target, reach and inventory space again when work completes.
  If a target burns, disappears or becomes unreachable, cancel and choose again.
- Construction takes time. Reserve the recipe resources and site at job start;
  consume the resources once on completion. Cancellation releases reservations.
  No unfinished building actor is needed for the first version.

### Autonomous choices

Use a small ordered goal selector and an action state machine. A general
planning framework or external AI service is unnecessary for this milestone.

Proposed priorities:

1. Leave an unsafe work location and abandon invalid targets.
2. Finish a valid action instead of changing goals every update.
3. Obtain missing supplies and build a house if the settler has no home.
4. Obtain wood and build a ship after establishing a home.
5. Board an available ship and choose a reachable coastal destination for a
   short sailing trip; land again rather than generating endless new ships.
6. Idle when no valid work exists, with bounded retries after relevant changes.

Keep the goal, current action, target, progress and blocked reason inspectable.
Use stable actor IDs for ties and reservations, and seeded choices where variety
is useful. Decisions must not depend on render frames or Flecs iteration order.
Propose one owned house and one single-passenger ship per settler initially;
shared households, crews and cooperation can follow later.

### Terrain, pathfinding and continuous movement

- Add seeded inland mountain patches and a distinct terrain appearance. Exclude
  mountains from tree placement, construction sites, walking and landing.
- Spawn settlers on walkable terrain with reachable resources, building space
  and a shore. A map with no valid spawn must report that condition without
  placing a settler on a mountain or in water.
- Use A* over neighboring hexes for route planning, with separate walking and
  sailing traversal rules. Mountains and water block walking; land and mountains
  block sailing. Neither mode may leave the map.
- Treat the route as a navigable corridor. Follow continuous GLM world-space
  positions and validated waypoints, with safe line-of-sight smoothing where
  possible. Tile centers are planning aids, not mandatory stopping points.
- Check the moving body's clearance along every segment, including smoothed
  segments. Never cut through a mountain corner, shore or building footprint.
  Work destinations are reachable positions within interaction range.
- Keep building footprints separate from dynamic actor locations. Multiple
  settlers can occupy a tile; propose allowing them to pass one another before
  adding crowd avoidance. Do not block an entire tile just because a settler is
  standing there.
- Replan when a route or destination becomes invalid, with bounded search and
  retries. A blocked goal must not freeze the simulation or teleport the actor.
- Move settlers and ships at the existing 32 Hz fixed simulation rate; keep
  higher-level choices/work rules at 4 Hz where appropriate. Use remaining
  movement distance across waypoints, so speed is independent of route density.

### Houses and fire

- Choose an unoccupied, reachable land site and reserve its footprint and a
  worker approach location. Prevent construction on water, mountains or an
  active fire, and prevent overlapping houses or trapping a worker inside one.
- Record home ownership when construction finishes. Inventory remains personal;
  a house does not introduce storage or food production implicitly.
- Extend combustion beyond tree-only queries: trees can ignite houses and
  burning houses can ignite nearby combustible actors. Manual ignition should
  also work on houses. Keep staged spread and seeded burn durations.
- At burnout, remove the house, release its footprint, invalidate stale jobs and
  clear its owner's home reference. The settler can choose to rebuild when
  supplies allow. Terrain recovery must not recreate a house.
- Share fire state/rules where useful while keeping tree and house outcomes
  explicit. Settlers avoid burning work sites; damage to settlers and fire
  affecting ships are outside this first version.

### Ships and transitions

- Build from carried wood at a reachable shore, reserving both a land work
  position and an adjacent, unobstructed water launch position.
- Start with one passenger. A ship is a separate Flecs actor with a boarding
  reservation and passenger reference; a settler records the carrier handle.
- Board only while the ship is alongside reachable shore. Transfer to an
  explicit aboard state; the ship owns movement while the passenger's displayed
  position follows it. Never run land movement for the passenger simultaneously.
- Sail continuously along a valid water route. Select reachable landing points
  around the current island; do not require a second island to test the feature.
- Disembark only alongside an accessible, clear land position. If landing is
  blocked, remain aboard and choose another destination or wait safely.
- Regeneration clears passengers, routes and reservations along with other
  actors. Unexpected carrier deletion must resolve the passenger safely; do not
  leave a dangling handle or place a walking settler in open water.
- Exclude multi-island generation, docks, ship combat, sinking and crews for now.

## Fit with the current architecture

Flecs remains the authoritative store for trees, settlers, houses and ships.
Keep components/factories in `src/actors/`, AI, work, navigation and movement
systems in `src/simulation/`, terrain and hex geometry in `src/world/`, drawing
in `src/render/`, particles in `src/effects/`, and inspectors/tools in `src/gui/`.
Game logic uses GLM and has no SDL or ImGui dependency. Keep build definitions
in the root `CMakeLists.txt`.

The current occupancy array holds one actor per cell and fire assumes that actor
is a tree. Replace that assumption with explicit static occupancy/reservations
and queries for combustible actors. Moving actors must not overwrite trees or
house footprints. Generalize actor value views, selection, drawing and fire
emitters for the new kinds without creating a second authoritative actor store.

Retain versioned actor handles, stable gameplay IDs and the shared fixed clock.
Document system order: validate handles/reservations, apply staged fire outcomes,
choose work and routes, move actors, validate/commit completed actions, then
refresh occupancy and presentation views. Fire and work targeting the same tree
must have a deterministic result, never duplicate wood or a destroyed target
being used to finish construction. Pause and speed affect every system equally.

## Implementation milestones

1. **Terrain and navigation:** mountains, traversal rules, A* and segment
   clearance tests, including unreachable destinations.
2. **Settler actors and movement:** valid spawning, continuous movement,
   read-only inspection and routes on a small deterministic scenario.
3. **Inventories and tree cutting:** timed work, wood yields, reservations and
   cancellation; connect autonomous resource choices.
4. **Food and homes:** resolve food rules, obtain supplies and autonomously
   construct a house. Inspect goals, inventory and home ownership.
5. **House fire:** spread between trees and houses, burnout and rebuilding;
   update effects and selection to handle destruction.
6. **Ships:** coastal construction, boarding, water navigation, landing and
   autonomous trips around the current island.
7. **Integration:** pause/speed/regeneration, resource exhaustion and blocked
   goals; update project status and intentional generation/runtime fixtures.

## Decisions to settle before the relevant milestone

- Food supply: initial carried provisions only, or a simple gatherable food
  source? Starting provisions are smaller in scope but finite; depletion must
  produce a visible blocked goal rather than free resources.
- House recipe: does sufficient food act as a retained stock requirement or is
  it consumed as a construction provision? Choose amounts alongside wood costs.
- Confirm population, personal capacity, ownership/passenger defaults and visual
  assets before implementing their milestones. Placeholder art is sufficient
  for simulation validation; hunger and health remain deferred.

## Acceptance and validation

- A deterministic scenario shows settlers choosing work, walking around mountains,
  cutting trees, receiving wood once and building a house only with enough food
  and the agreed material costs. Inventory cannot become negative or duplicate.
- Two workers competing for a target/site cannot both consume it. Fire, target
  deletion, lost paths and regeneration cancel stale actions and reservations.
- Continuous paths and smoothed segments respect terrain/building clearance,
  including at high simulation speed. Unreachable targets do not cause busy loops.
- A house catches fire from a tree, can spread fire onward, burns down and frees
  its site. The owner remains alive without introducing health or hunger rules.
- A settler builds and boards a ship, sails only over water, follows the ship
  while aboard and lands on accessible land. Blocked landings remain safe.
- Pause freezes work/movement/fire; replay is independent of frame grouping and
  actor query order. Regeneration clears jobs, ownership, routes and passengers.
- Add domain regressions for these behaviors and rendering/picking checks for the
  new actor kinds. Run root Debug/Release builds and CTest presets per milestone;
  visually review movement, mountain readability, house fire and boarding.
