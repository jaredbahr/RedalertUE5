# Drag-box selection

Implementation is prepared for source review. No local compile, native automation,
engine, screenshot, or physical-input acceptance has been run for this integration.
The independently reviewed planner is retained unchanged from
`fc69573c9a6fb377246735048f43523f320eec77`.

## Interaction contract

- In a local playing skirmish, left press arms without changing selection. It
  records owner, epoch, phase, controller, viewport identity and dimensions,
  pointer position, Shift, and the press-time click target.
- Movement of at least four Euclidean viewport pixels makes the gesture a drag.
  Release is sampled too. Once crossed, returning to the start remains a box.
  This threshold is a prototype default, not a measured RA2 interaction constant.
- A shorter gesture selects or Shift-toggles the revalidated press target on
  release. Structures remain click-selectable. Legacy non-skirmish clicks and
  shared fixture actions keep their existing routes.
- Box hits use current projected ground-position centers, not mesh bounds or
  occlusion. Only live, finite, owned Combat/Harvester records qualify. Centers
  must lie in the half-open viewport, outside the existing HUD panel, and within
  the closed normalized box. No drag-direction policy difference is applied.
- Normal box replaces and chooses the lowest ID as primary. Shift-box preserves
  valid existing selection, including structures and the valid current primary,
  then adds sorted new mobile hits. Final IDs are sorted and unique, bounded by
  the current selection cap of 12. This is not an army-size or performance claim.
- The planner receives the full cap, including on Shift-box. A valid empty normal
  box clears; Shift-empty preserves valid current selection. An invalid plan
  never commits. The atomic selection API checks expected versus actual owner
  and epoch, current selection context, player presence and playing phase, stages
  all changes, then advances the explicit selection revision once. Normal
  snapshot synchronization/pruning is independent.

## Cancellation and scope

Missing controller/state/viewport, focus or foreground loss, invalid/outside
pointer, HUD interaction, resize, owner/epoch/phase or viewport/controller change
cancel the pending gesture. Type selection, groups, direct shared click selection,
right-click orders, production/build/stop/rematch input, zoom and interactive exit
also cancel it before following their existing action. Cancellation hides the
outline immediately and does not resume while held. Observed button-up or a
release must precede a fresh press. A lost release is discarded on an up
observation, never synthesized into a click. Duplicate releases are inert.

The outline uses the existing yellow two-pixel selection style. Selection does not
submit commands, mutate the snapshot, or clear pending group orders. Command
model, RPCs, bot logic, nine groups, context orders and order queue are unchanged.
Native callback counters remain separate from shared fixture calls; none proves
physical input or human acceptance.

## Acceptance still required

1. Independently review the combined gesture, binding, cancellation, HUD and atomic
   API delta, including the exact production call sites and test expectations.
2. After review clears, compile the production module and run all exact named
   controls tests, including `BoxSelectionAtomic`,
   `BoxSelectionContextAndCapacity` and `BoxSelectionGesture`. The runner retains
   all 16 earlier expected tests and its existing timeout/CPU policy.
3. Exercise bound press/drag/release and cancellation in engine at real viewport
   dimensions, then check actual human clicks, Shift, structures, HUD exclusion,
   focus loss/refocus, resize, match transition, and existing commands/groups.
   Keep synthetic callback evidence distinct from human input and screenshots.

Prepared model tests cover atomic replacement/addition, invalid versus empty,
ownership/life/finite filtering, structure preservation, overlapping cap hits,
primary fallback, context rejection, snapshot immutability, release-only and
diagonal threshold crossing, sticky drag, duplicate edges and cancellation.
They are source assertions awaiting execution, not passing test results.
