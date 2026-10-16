# Selection, groups and optional camera pan

This source-only slice builds on the small skirmish and session lifecycle branches. Its goal is familiar RTS control of the current generic units. It does not establish RA2 gameplay parity or approved unit scale/art.

## Controls

- Left-click selects one owned live unit; empty ground clears selection. Shift-click toggles an owned live unit in the selection (up to 12).
- Ctrl+1, Ctrl+2 and Ctrl+3 save independent copies of the selection. Press 1, 2 or 3 to recall. Dead/foreign units are filtered; a new match epoch or owner clears all selections and groups.
- With multiple units selected, right-click ground moves Combat and Harvester units while preserving their current offsets around the destination. The destination is bounded to the proof world. Structures stay put.
- Right-click an enemy with a group orders only Combat units to attack. Friendly teammates are excluded in skirmish. Harvesters do not attack. Select a single Harvester to use the existing right-click ore gathering action.
- Wheel zoom and the default orthographic camera angle/starting view remain as before. Optional arrow-key pan requires `-CameraPan` on `start-small-match.ps1` or `start-local-interactive.ps1` (engine flag `-ProofCameraPan`). It runs only while the viewport is focused and foreground, at 600 cm/s, with normalized diagonals, a 0.1-second frame cap, and a bounded ground center. It does not rotate the camera.

The HUD highlights every selected unit and shows the selected count. Group assignment/recall is local presentation state; it does not send gameplay commands.

## Authority and bounded dispatch

Group planning reads replicated snapshots. It does not mutate units, credits, damage or resources. Each intent still goes through the existing pawn `Request` method and the server's normal owner, epoch, sequence and ingress checks. Sequence numbers are allocated when an intent is actually sent.

The client retains at most 64 pending Move/Attack intents. A new order for a pending unit replaces that unit's earlier intent while preserving orders for other groups. Overflow rejects the new batch. At most two intents are released per 0.1 wall-clock seconds, without catch-up bursts. Ownership, life, unit role, target team and match phase are checked again before release. A later single-unit Move/Attack/Gather cancels that unit's pending group intent. Epoch/owner changes clear the queue. This pacing does not replace or relax the server ingress limit; other manual command types still share that existing limit.

Formation offsets preserve the units' current arrangement; there is no navigation, collision avoidance, formation editor, box selection or attack-move in this slice.

## Verification status and next gate

**UE compilation, new automation tests, rendered controls and network regressions have not run for this branch.** The shared heavy-test slot has returned; the lifecycle fixture rebuild and Coop rerun passed first at clean head `9f2d562` (91.058 seconds, winner0). Source review, PowerShell syntax parsing and whitespace checks do not prove runtime behavior or physical input.

Ten new automation tests cover selection ownership/pruning, group copies and epoch/owner invalidation, deterministic bounded plans, queue coalescing/capacity/pacing/revalidation, and camera math including the actual camera basis. `unreal/Scripts/test-group-controls.ps1` is prepared to require those exact ten successes in a hidden NullRHI editor, using the existing two-CPU/Normal-priority/deadline/cache/owned-process safeguards.

Next, prepare this source-only worktree using existing ignored generated content, build serially with the existing one-action/two-CPU script, run the new controls tests plus existing model, controls and network gates, and inspect rendered HUD evidence. Retain failures and the actual compiled source commit in the result ledger. A human should check Shift-click, Ctrl groups, group orders and optional pan in the bounded interactive launcher after automated gates pass; no visible game was launched for this preparation.
