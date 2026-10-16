# Manual UE command proof

This is a small engine-native correctness fixture, not a playable RA2 roster or a scale benchmark. Use the runtime proof launcher and generated gameplay map described in the [UE test plan](UE_TEST_PLAN.md). The existing `VisualProof` map in [UE setup](UE_SETUP.md) is the separate three-asset art review scene. No art approval is recorded.

## Controls

- Left-click within 40 screen pixels of an owned live unit's ground marker to select it. Clicking elsewhere clears selection.
- Right-click ground to request movement, or an enemy marker to request attack. The ground plane is world Z=0; no navigation or terrain raycast is implied.
- Press **P** to request one generic unit using the fixture's toy credits and production rule. Production does not require a building in this proof.
- Use the mouse wheel to change the fixed oblique orthographic camera width. The camera stays centered on the fixture origin; panning is not implemented.

Click the game viewport first if the editor has keyboard focus. Green markers identify owned units, red markers identify other owners, and yellow brackets identify the selected owned unit. The HUD shows the host-assigned owner, replicated snapshot revision, toy credits, selection, the last request sent, and a separate host **ACCEPTED/REJECTED** receipt with its sequence and reason. A receipt does not predict a state change: the views and credits still update only from snapshots.

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

## Bounded engine-rendered controls scenario

After building the native module and generating the CommandProof map, run:

```powershell
& .\unreal\Scripts\test-controls-proof.ps1
```

The runner first performs a separately bounded standalone graphical warmup (`-game`, `-RenderOffscreen`, 1280x720, `-ExecCmds="quit"`), using a reusable cache at `generated/controls-cache` on D:. Warmup must exit successfully with RHI/map initialization evidence within 180 seconds. It claims no screenshot or controls acceptance. Installed UE source queues `ExecCmds` during `UEngine::Init` (`Engine/Private/UnrealEngine.cpp`, around line 2248); the engine loop executes deferred commands after initialization (`Launch/Private/LaunchEngineLoop.cpp`, around line 6078).

Only after warmup succeeds does the runner start the strict scenario clock and launch one dedicated NullRHI host, one actual rendered UE client at 1280x720 (`-RenderOffscreen`), and one passive NullRHI observer. It waits for the rendered client to acquire owner 1 and a valid viewport before admitting owner 2. The scenario remains bounded to 90 seconds; the combined process runtime budget is 180 + 90 = 270 seconds, plus polling/cleanup overhead. Parameters may lower those limits. Cache/temp overrides remain process-local, each process receives two cores, and cleanup is restricted to processes this run started. It changes no firewall, global environment, security setting or installation. Existing cache entries are reused without deleting prior evidence or other project files.

`-ProofControlsAutomation` is separate from the original `-ProofAutomation` RPC smoke. The rendered pawn projects snapshot positions into its actual viewport and supplies those screen coordinates to the **same** selection and right-click handlers used by manual controls. Both paths share the pointer resolver and ground `DeprojectScreenPositionToWorld` calculation. Assertions cover enemy selection refusal, owned selection, empty-space clearing, zoom in/out, projection/deprojection round trip within 2 cm, accepted movement and destination arrival, accepted attack and replicated target damage, and production with the exact host-owned unit/credit delta. All orders use controller RPCs and host receipts; no test-only state mutation or direct model submission occurs. A fourth accepted move separates the original unit from its new spawn for the final screenshot.

The host independently commands two bot units through normal authoritative submission: a mover must reach its destination, and an attacker damages owner 2's passive unit. Rendered owner 1 attacks a separate stationary owner 100 target, so its attack cannot substitute for bot damage. The fixture begins with two human-owned units and three bot-owned units. One production request adds a sixth record; dead records remain in snapshots. Final counts describe the 20-second scenario observation boundary, before client/process shutdown. These tiny observed counts do not establish an army-size budget.

Fresh output goes under `generated/controls-<UTC timestamp>/`: warmup/host/rendered/observer logs, `result.json`, and two requested UE screenshots (`01-selected.png`, `02-moved-combat-produced.png`). The runner requires successful scenario markers/assertions, all four accepted receipts, at least three equal snapshot digests shared by all scenario processes, observed initial/final owner/unit counts, and fresh 1280x720 PNGs with valid signatures and nontrivial byte sizes. JSON records tested HEAD, dirty-source flag, UTC start/end, separate warmup/scenario/total stopwatch durations and timeout budgets, stages, reusable cache path, observed counts, common revisions, screenshot hashes and any failure. File checks establish capture provenance; a person must inspect the images for HUD/marker readability and scene contents.

This is **engine-rendered functional automation**, not physical mouse, keyboard, window focus or touch validation. It supplies viewport coordinates through a scenario cursor rather than moving the operating-system pointer. Native UI automation is unavailable in this environment. Manual viewport focus and actual device inputs remain unchecked. Host rejection text is surfaced by the HUD, but this finite scenario tests accepted commands; resource rejection remains in the manual checklist and core tests. No run is claimed until its logs, JSON and rendered images have been reviewed.

## First cold-start attempt

At tested clean HEAD `4689d5e8c02dd1f62eaef8ed3419d0c5490a69d1`, run `generated/controls-20261016-044931-666` failed after **90.358 seconds**, waiting for rendered viewport readiness. The host became ready; the graphics client initialized D3D12/SM5 on the RTX 3080 but did not reach its first gameplay frame before timeout. No controls assertions, observed scenario counts or screenshots passed. The log reported one shader worker, and the fresh run-local cache received 262 shader-job files during startup. Cold shader initialization exceeding the limit is the working diagnosis, not a proven graphics-driver fault. That failed evidence remains intact. The reusable cache and separate warmup address this setup cost without loosening scenario assertions; their success still requires a new recorded run.
