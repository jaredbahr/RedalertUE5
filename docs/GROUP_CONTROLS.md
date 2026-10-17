# Selection, groups and optional camera pan

This source-only slice builds on the small skirmish and session lifecycle branches. Its goal is familiar RTS control of the current generic units. It does not establish RA2 gameplay parity or approved unit scale/art.

## Controls

- Left-click selects one owned live unit; empty ground clears selection. Shift-click toggles an owned live unit in the selection (up to 12).
- T replaces the current selection with live owned units of the primary unit's current generic kind whose projected centers are inside the viewport, up to 12. A visible primary keeps a slot; other slots use the lowest unit IDs. With no live primary, T does not expand the selection. Saved groups and pending commands stay unchanged. Repeated T does not expand across the map.
- Ctrl+1 through Ctrl+9 save independent copies of the selection. Press 1 through 9 to recall. Dead/foreign units are filtered; a new match epoch or owner clears the current selection and all nine groups. Assigning an empty selection clears that slot.
- With multiple units selected, right-click ground moves Combat and Harvester units while preserving their current offsets around the destination. The destination is bounded to the proof world. Structures stay put.
- Ctrl+Shift+right-click ground plans Attack Move for selected owned live Combat units in a playing skirmish. They travel toward their destinations, stop to fire at enemies within the existing weapon range, and resume when targets die or leave range. S cancels the route; ordinary Move and Attack replace it. This is a scoped prototype without out-of-range pursuit, Guard, fog or RA2 balance claims. The original manual supports Ctrl+Shift but does not specify the mouse button; right-click follows this proof application's existing order convention.
- Right-click an enemy with a group orders only Combat units to attack. Friendly teammates are excluded in skirmish. Harvesters do not attack. Select a single Harvester to use the existing right-click ore gathering action.
- Optional edge scrolling requires `-CameraEdgePan` on either interactive launcher (engine flag `-ProofCameraEdgePan`). It works in manual practice only, in the outer 16 pixels of the actual viewport, while focused and foreground. The HUD panel and any held left/right/middle mouse button suppress edge scrolling. It shares the existing 600 cm/s pan speed, normalized combined arrow/edge input, frame cap and ground-center bounds; it does not rotate or zoom. Both pan options remain off by default. This is source implementation pending build/rendered and physical usability acceptance.
- Wheel zoom and the default orthographic camera angle/starting view remain as before. Optional arrow-key pan requires `-CameraPan` on `start-small-match.ps1` or `start-local-interactive.ps1` (engine flag `-ProofCameraPan`). It runs only while the viewport is focused and foreground, at 600 cm/s, with normalized diagonals, a 0.1-second frame cap, and a bounded ground center. It does not rotate the camera.

The HUD highlights every selected unit and shows the selected count. Group assignment/recall is local presentation state; it does not send gameplay commands.

## Authority and bounded dispatch

Group planning reads replicated snapshots. It does not mutate units, credits, damage or resources. Each intent still goes through the existing pawn `Request` method and the server's normal owner, epoch, sequence and ingress checks. Sequence numbers are allocated when an intent is actually sent.

The client retains at most 64 pending Move/Attack intents. A new order for a pending unit replaces that unit's earlier intent while preserving orders for other groups. Overflow rejects the new batch. At most two intents are released per 0.1 wall-clock seconds, without catch-up bursts. Ownership, life, unit role, target team and match phase are checked again before release. A later single-unit Move/Attack/Gather cancels that unit's pending group intent. Epoch/owner changes clear the queue. This pacing does not replace or relax the server ingress limit; other manual command types still share that existing limit.

Formation offsets preserve the units' current arrangement; there is no navigation, collision avoidance, formation editor or box selection in this slice. Attack Move is the separately validated playing-skirmish action described above.

## Current validation

The [player-feature evidence](PLAYER_FEATURES.md) records the current 84-case/13-runtime engineering set and exact c54/42 bindings. Earlier results below remain revision-specific history. Hardware input/focus acceptance is still outstanding; 960-pixel primary annotation omissions keep issue #18 open.

## Verified source and remaining acceptance

### Nine-group follow-up: model and compatibility verified; new-digit input pending

See the [exact-commit integration review and next-slot plan](INTEGRATION_REVIEW.md) for sibling-branch compatibility, executed checks versus prepared tests, and the refreshed merge recommendation.

The isolated `feat/nine-control-groups` branch starts at PR #28 (`0173ff8d9004432857c9a012caf60ecd872e617a`). It expands the three-slot proof to the nine digit groups documented under Create Team / Select Team in the [original Yuri's Revenge manual, Quick Keys](https://archive.org/stream/Command_Conquer_RED_ALERT_2_YURI_S_REVENGE/Command__Conquer_RED_ALERT_2_YURI_S_REVENGE_djvu.txt). This is publisher-authored material transcribed on a third-party host. No manual or game assets are copied into the repository. Number 0 is outside this documented scope.

The selection model stores nine bounded local copies; all nine input callbacks use the existing Ctrl assign / plain recall handler. The selected-unit cap, command queue, host authority, movement/combat and camera behavior retain their existing rules. A new `SelectionNineGroups` test covers independent slots, copied primary IDs, invalid-slot no-ops, death/ownership pruning, and clearing upper slots when owner or match epoch changes. Existing controls tests remain required; the runner now expects exactly eleven successful cases.

The prepared bounded batch completed at clean `9bc6a4a0562fedd28203cf4d1c684bdb65ccce40` with installed UE 5.7.4. Source tree and controls-runner hashes match the integration plan. Six build actions passed in 46.79 seconds total / 36.72 executor. All jobs were serialized; no failed attempt or retry occurred in this batch. Reproduction commands for the primary gates are:

```powershell
./unreal/Scripts/build-native-proof.ps1
./unreal/Scripts/test-group-controls.ps1
./unreal/Scripts/test-controls-proof.ps1 -CleanPresentation
```

Results below are under ignored `D:/UserTemp/RedalertUE5-ten-control-groups/generated/`; later publication edits update documentation only.

| Gate / retained run | Result |
| --- | --- |
| `group-controls-model-20261016-234011-131` | 11/11, zero failures, 32.388 seconds; includes `SelectionNineGroups` |
| `native-model-20261016-234043-558` | 7/7, zero failures, 13.345 seconds |
| `skirmish-model-20261016-234056-941` | 14/14, zero failures, 13.519 seconds |
| `fidelity-model-20261016-234110-575` | 4/4, zero failures, 13.032 seconds |
| `controls-20261016-234130-989` | Pass, 63.586 seconds total, 21.002 warmup, 42.436 scenario; 55 matching shared revisions; two fresh 1280x720 captures; cleanup errors 0 |

Both rendered captures were inspected: the 1–9 hint and selected-unit highlight are readable; the later capture shows movement, accepted host receipt, a produced unit and target damage. The rendered fixture calls shared handlers directly: native callback counts remain zero and the HUD labels human input unverified. Independent evidence review found no blocker for this stated scope. The model tests and compatibility runner do not establish actual digit-key/Ctrl dispatch for new slots 4–9. Subsequent [draft PR #31](https://github.com/jaredbahr/RedalertUE5/pull/31) passed the focused engine-injected 4–9 save/recall, modifier-release and overwrite check at combined clean source bff79aede5d54ca9e850f134e17629954e046252. That is separate evidence from this standalone PR #30 batch. Human input/focus and higher-digit engine checks for death pruning/rematch remain separate acceptance work.

Fourteen previously validated ignored assets were reused with matching SHA256 hashes (`reused-assets-nine-groups.json`); no art was generated. Build/temp/model outputs and the rendered cache (`generated/controls-cache`) are on D:. The runners use normal priority, two-CPU affinity assignment/repair, 30 FPS offscreen rendering and their unchanged bounds. Warmup CoreLimit is 4; worker inheritance is unverified and polling is not hard confinement. This batch is not performance evidence or a zero-C:-writes claim. All owned UE/build processes exited; `resource-handoff.json` releases the shared slot.

This branch is independent of draft PR #29 and does not carry or validate its bootstrap fix. PR #29's original readiness timeout and separate stage-16 failure remain unresolved. Its later bootstrap source built, but final runtime attempts stopped before fixture execution; do not use its historical passes as nine-group validation.

Draft [PR #30](https://github.com/jaredbahr/RedalertUE5/pull/30) contains this follow-up; reviewed source is `c62e544200bfca44c180d349009cb6b98a42de49`. Related issues [#17](https://github.com/jaredbahr/RedalertUE5/issues/17) (input/focus usability) and [#18](https://github.com/jaredbahr/RedalertUE5/issues/18) (HUD readability) remain partial and open. Nine groups address a narrow controls gap; they do not satisfy physical-input/focus or full presentation acceptance. PR #30 depends on #28 and should be reconciled alongside #29 after integration, without merging either here.

### Historical three-group validation

All final tests below ran with clean compiled source `3409da2e7ba94952e8568564b1a884fd15e480bd`, using installed UE5.7.4. The last build succeeded in four actions / 8.55 seconds total. Later commits update documentation only. Tests were serialized using the established bounded runners, D: cache/temp paths, Normal-priority policies and owned-process cleanup. Model/scenario jobs used two logical CPUs; the existing rendered controls warmup used four. Policy polling is not hard confinement.

Result directories are under ignored `D:/UserTemp/RedalertUE5-group-controls/generated/`:

| Gate | Result | Directory |
| --- | --- | --- |
| New selection/group/queue/camera tests | 10/10, zero failures, 11.982s | `group-controls-model-20261016-214922-445` |
| Existing command model tests | 7/7, zero failures, 11.986s | `native-model-20261016-214934-455` |
| Existing skirmish model tests | 14/14, zero failures, 12.386s | `skirmish-model-20261016-214946-480` |
| Native host/two-client proof | Pass, 60 matching common revisions | `native-network-20261016-215012-438` |
| RPC flood close/healthy-client isolation | Pass, 18.814s | `rpc-ingress-20261016-215044-751` |
| Offscreen rendered controls compatibility | Pass, 58.432s including warmup; 54 matching revisions | `controls-20261016-215103-604` |
| Offscreen cooperative small match | Pass, 102.577s; 331 matching revisions; winner0; no cleanup errors | `small-match-coop-20261016-215202-072` |

The new unit tests cover ownership/pruning, group copies and epoch/owner invalidation, deterministic bounded plans, queue coalescing/capacity/pacing/revalidation, and camera math derived from the actual camera basis. At that tested revision, `unreal/Scripts/test-group-controls.ps1` required those exact ten successes. The existing rendered fixture exercises single-unit compatibility; it does not exercise native Shift/Ctrl input or prove multiple selection highlights and queued group RPCs together in a rendered session. Physical input, focus transitions, optional panning and subjective responsiveness still need a human playtest. No visible game was launched. No FPS, Internet or large-army result is claimed.

Rendered controls and cooperative victory/HUD captures were visually inspected locally. Fourteen existing ignored generated content files were reused with matching SHA256 hashes; no art was generated/imported. The eight monitored C: roots showed UE5.7 Saved +31 bytes since the post-lifecycle Coop observation; the other seven were unchanged (`c-drive-after-controls.json`). This shared-PC audit is neither exhaustive nor exclusive attribution. All binaries, caches, reports and PNGs remain ignored on D:.

Retained attempts: `build-group-controls.log` failed at `f17929b` in 40.25s because unnamed helper functions collided in UE unity compilation and a test helper collided with the `Move` overload. Distinct helper names fixed both; `build-group-controls-fixed.log` succeeded at `ed61f4c` in 10.75s. The first automation run (`group-controls-model-20261016-214816-930`) recorded 9/10 successes and one failure in 18.431s: rotation-matrix basis comparisons used a tighter tolerance than UE trig precision. The camera behavior was unchanged; the test now records the basis and uses 0.000001 for unit axes / 0.0001 cm for matrix-derived movement comparisons. The clean rebuilt run passed all ten. None of these logs/reports was deleted or counted as a successful final gate.

The prior lifecycle fixture gate was completed first: clean `9f2d562` rebuilt successfully and passed Coop in 91.058s with 332 matching revisions and winner0. See [lifecycle evidence](SESSION_LIFECYCLE.md). Remaining integration and gameplay-fidelity work is sequenced in [Milestones](MILESTONES.md).

Focused edge policy checks: `unreal/Scripts/test-camera-edge-model.ps1` runs exactly three `RedalertProof.CameraEdge` cases (geometry, pointer ownership and invalid input). These do not prove native pointer delivery, physical focus behavior or rendered projection. Existing group-controls test inventory stays at eleven.

The opt-in rendered fixture is `test-small-match.ps1 -Scenario Victory -ReadabilityCaptures -CameraEdgeCaptures -CleanPresentation`. It retains the existing 240-second maximum, adds fresh `06-camera-before.png` / `07-camera-after.png`, and exercises ten fixed synthetic right-edge steps through the same rig-translation method used by manual panning. A later-frame projection must shift left by more than 10 pixels and match its baseline-projection prediction within 2 pixels, with a 600 cm rig translation, unchanged zoom/rotation, mesh world transforms and frozen authoritative digest. These checks remain unrun until the root validation batch. They do not establish physical pointer/focus delivery or commands issued after panning.
### Visible type selection: prepared source

T follows the first-use on-screen Type Select idea in the [original Red Alert 2 manual, page 23](https://www.scribd.com/document/760314562/Red-Alert-2-English-3-Manual), publisher-authored material mirrored by a third party. This prototype groups `Combat`, `Harvester`, `HQ`, `Refinery` and `Factory`; it does not distinguish the full RA2 unit/faction roster. Repeated/global expansion is deferred.

Here, visible means a finite projected unit center inside `[0,width) × [0,height)` for the actual viewport. This is not an occlusion, fog-of-war, full-mesh or HUD-overlap test. An offscreen primary can supply the kind but is not retained; the lowest eligible visible ID becomes primary. A valid primary with no visible matches produces an empty selection. Invalid/dead anchors only undergo normal selection pruning. Owner/epoch changes retain their existing selection/group clearing rules.

Three prepared `RedalertProof.TypeSelection` automation cases cover filtering and primary retention, deterministic cap/context behavior, and viewport-center bounds. Source and tests have not been built or executed for this slice. The bound T callback records its own input counter; calling the shared handler or pure helper does not. No physical T input, engine-injected T dispatch, rendered selection, focus, performance or multiplayer acceptance is claimed here.

The dedicated `unreal/Scripts/test-type-selection.ps1` runner requires exactly those three successes and keeps a fresh report and hashes. `test-controls-proof.ps1 -TypeSelection` additionally prepares an opt-in shared-handler rendered proof after the unchanged Move/Attack/Produce baseline. It requires two live owned Combat records with projected centers in the viewport, an exact two-ID expansion with primary/group preservation, unchanged native-T count/command sequence/queue, two extra fresh 1280x720 PNGs, and at least three matching host/rendered/observer checkpoints spanning one actual host simulation second with stationary selected units and no extra owner-1 command ingress. Type and Stop fixtures cannot run together. The normal 55-second client, 24-second host, 23-second observer and 90-second runner bounds remain in place. Diagnostic rows are capped at 1024 per producer; overflow fails.

Twenty-one synthetic offline acceptance checks pass for this prepared evidence policy. They are parser tests only: the new UE automation and rendered gates remain unbuilt and unrun. The rendered proof calls the shared T action directly; it does not inject a native T event or verify physical input, foreground focus, Internet play or performance. Its viewport-center rule also does not establish visibility through fog, occlusion or HUD overlap. No screenshots or runtime acceptance for this feature are claimed yet.

Attack Move source checks are prepared in `test-attack-move-model.ps1` (exactly eight `RedalertProof.AttackMove` cases). Build, UE automation, replicated runtime and native modifier gesture acceptance remain pending. Enum ordinal 8 follows the separately implemented SetRally ordinal 7; integration must preserve both. Source basis: [original RA2 English manual, printed p21, Attack Move](https://www.scribd.com/document/760314562/Red-Alert-2-English-3-Manual).
