# Simplify tree lifecycle

Status: deferred. No gameplay changes implemented yet.

## Requested behavior

- Remove the growing and dead `TreeStage` values. Trees start fully grown;
  remove `TreeStage` entirely if the mature value no longer serves a purpose.
- Trees can die only from fire. Remove aging-based natural death.
- Disable natural tree births, growth and regrowth for now. Initial world
  generation still places fully grown trees.
- Settler planting and growth belong to a later milestone.

The later [settler task](settlers.md) adds deliberate tree harvesting. That may
remove healthy trees for wood; it does not restore natural death or regrowth.

## Implementation scope

Simplify tree components, factories and Flecs forest systems, then update the
inspector, counts, sprite selection and affected tests. Keep fire/burning state
and charred-remains handling separate from the removed lifecycle stages.
Preserve fire spread, burnout, remains cleanup and terrain recovery; terrain
recovery must not create new trees.

## Acceptance

- Without fire, generated trees persist and no new trees appear over time.
- Fire still kills trees and cleans up burned remains without using `TreeStage`.
- Regeneration places fully grown trees and clears previous fire state.
- Update deterministic fixtures and lifecycle regressions for the new behavior;
  run the Debug and Release builds and CTest presets.
