# Selection, groups and optional camera pan

This source-only slice builds on the small skirmish and session lifecycle branches. Its goal is familiar RTS control of the current generic units. It does not establish RA2 gameplay parity or approved unit scale/art.

## Controls

- Left-click selects one owned live unit; empty ground clears selection. Shift-click toggles an owned live unit in the selection (up to 12).
- Ctrl+1 through Ctrl+9 save independent copies of the selection. Press 1 through 9 to recall. Dead/foreign units are filtered; a new match epoch or owner clears the current selection and all nine groups. Assigning an empty selection clears that slot.
- With multiple units selected, right-click ground moves Combat and Harvester units while preserving their current offsets around the destination. The destination is bounded to the proof world. Structures stay put.
- Right-click an enemy with a group orders only Combat units to attack. Friendly teammates are excluded in skirmish. Harvesters do not attack. Select a single Harvester to use the existing right-click ore gathering action.
- Wheel zoom and the default orthographic camera angle/starting view remain as before. Optional arrow-key pan requires `-CameraPan` on `start-small-match.ps1` or `start-local-interactive.ps1` (engine flag `-ProofCameraPan`). It runs only while the viewport is focused and foreground, at 600 cm/s, with normalized diagonals, a 0.1-second frame cap, and a bounded ground center. It does not rotate the camera.

The HUD highlights every selected unit and shows the selected count. Group assignment/recall is local presentation state; it does not send gameplay commands.

## Authority and bounded dispatch

Group planning reads replicated snapshots. It does not mutate units, credits, damage or resources. Each intent still goes through the existing pawn `Request` method and the server's normal owner, epoch, sequence and ingress checks. Sequence numbers are allocated when an intent is actually sent.

The client retains at most 64 pending Move/Attack intents. A new order for a pending unit replaces that unit's earlier intent while preserving orders for other groups. Overflow rejects the new batch. At most two intents are released per 0.1 wall-clock seconds, without catch-up bursts. Ownership, life, unit role, target team and match phase are checked again before release. A later single-unit Move/Attack/Gather cancels that unit's pending group intent. Epoch/owner changes clear the queue. This pacing does not replace or relax the server ingress limit; other manual command types still share that existing limit.

Formation offsets preserve the units' current arrangement; there is no navigation, collision avoidance, formation editor, box selection or attack-move in this slice.

## Verified source and remaining acceptance

### Nine-group follow-up: source prepared, UE validation queued

See the [exact-commit integration review and next-slot plan](INTEGRATION_REVIEW.md) for sibling-branch compatibility, executed checks versus prepared tests, and the refreshed merge recommendation.

The isolated `feat/nine-control-groups` branch starts at PR #28 (`0173ff8d9004432857c9a012caf60ecd872e617a`). It expands the three-slot proof to the nine digit groups documented under Create Team / Select Team in the [original Yuri's Revenge manual, Quick Keys](https://archive.org/stream/Command_Conquer_RED_ALERT_2_YURI_S_REVENGE/Command__Conquer_RED_ALERT_2_YURI_S_REVENGE_djvu.txt). This is publisher-authored material transcribed on a third-party host. No manual or game assets are copied into the repository. Number 0 is outside this documented scope.

The selection model stores nine bounded local copies; all nine input callbacks use the existing Ctrl assign / plain recall handler. The selected-unit cap, command queue, host authority, movement/combat and camera behavior retain their existing rules. A new `SelectionNineGroups` test covers independent slots, copied primary IDs, invalid-slot no-ops, death/ownership pruning, and clearing upper slots when owner or match epoch changes. Existing controls tests remain required; the runner now expects exactly eleven successful cases.

Only source/static review is available for this follow-up. It has not been built or executed in UE; the historical evidence below validates the earlier three-slot source only. Imagination owns the shared heavy-test slot. When that slot is released, run the existing bounded build, focused controls tests, then rendered compatibility in this D: worktree:

```powershell
./unreal/Scripts/build-native-proof.ps1
./unreal/Scripts/test-group-controls.ps1
./unreal/Scripts/test-controls-proof.ps1 -CleanPresentation
```

The model test and rendered compatibility runner do not establish actual digit-key/Ctrl dispatch for new slots 4–9. A bounded engine-input check and human review of assigning/recalling groups 4 and 9, Ctrl release, group death pruning and rematch clearing remain acceptance work. No generated content is included in this new worktree, so the rendered step also requires the previously validated ignored fixture assets to be reused and hash-checked before launch.

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
