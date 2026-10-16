# Manual UE command proof

This is a small engine-native correctness fixture, not a playable RA2 roster or a scale benchmark. Use the runtime proof launcher and generated gameplay map described in the [UE test plan](UE_TEST_PLAN.md). The existing `VisualProof` map in [UE setup](UE_SETUP.md) is the separate three-asset art review scene. No art approval is recorded.

## Controls

- Left-click within 40 screen pixels of an owned live unit's ground marker to select it. Clicking elsewhere clears selection.
- Right-click ground to request movement, or an enemy marker to request attack. The ground plane is world Z=0; no navigation or terrain raycast is implied.
- Press **P** to request one generic unit using the fixture's toy credits and production rule. Production does not require a building in this proof.
- Use the mouse wheel to change the fixed oblique orthographic camera width. The camera stays centered on the fixture origin; panning is not implemented.

Click the game viewport first if the editor has keyboard focus. Green markers identify owned units, red markers identify other owners, and yellow brackets identify the selected owned unit. The HUD shows the host-assigned owner, replicated snapshot revision, toy credits, selection, and the last *request sent*. That message is not an acceptance receipt: a rejected request must leave the next authoritative snapshot unchanged.

Each client renders local, non-colliding static-mesh components from replicated unit records. Position, health, credit balance, production, ownership and death come only from the host snapshot; the presentation code does not predict or simulate them. Dead records remain in the host model but their views and selection disappear. The player controller supplies increasing request sequence numbers shared by its command paths; respawn/reconnection sequencing is outside this fixture. Manual orders are disabled when `-ProofAutomation` is active so they cannot disturb the scripted acceptance sequence.

If the local original generated Rhino mesh exists at `/Game/BlockoutGenerated/Meshes/SM_RhinoTank_Blockout`, every generic record uses that same mesh at **0.3 display scale** for the tiny command fixture. Otherwise it uses the engine basic cube. The records have no unit-kind field, so these views do not demonstrate infantry/vehicle differentiation, approved army scale, rigging, or finished art. The three-asset visual proof remains the place to review the original Tesla Reactor/Rhino/Conscript together.

## Manual acceptance checklist

Run a host plus two actual UE clients with graphics after the core and network smoke checks pass. This checklist requires visual/input inspection; NullRHI or successful compilation alone cannot pass it.

1. Both clients show their distinct assigned owner IDs, the same host revision/units, readable markers, and ground under the camera. The view works with generated assets and with the documented cube fallback.
2. Each client selects only its own living unit. Right-click ground: its requested movement appears in subsequent snapshots and both clients show the same final location. No local teleport should precede a snapshot update.
3. Right-click an enemy marker: host-owned combat changes health consistently on both clients. A dead unit's view disappears and cannot stay selected.
4. Press P: only the host changes credits and creates the unit, and both clients display the result. When credits are insufficient, the host rejects production without a local spawn or credit change.
5. Mouse wheel changes only the local view. Losing selection or clicking empty space produces no gameplay changes. Record any focus, projection, readability or HUD overlap problems as observed bugs.

Status: compilation, four native command test groups and the host/two-client NullRHI replication fixture passed; see [recorded evidence](UE_NATIVE_PROOF.md). No graphical pass or screenshot is claimed by this document. Internet traversal, packet loss/latency handling, eight players, large armies, pathfinding and a complete bot skirmish remain separate gates.
