# Engine input binding evidence

Run from this D: checkout with the installed UE 5.7.4/toolchain:

```powershell
./unreal/Scripts/build-native-proof.ps1
./unreal/Scripts/test-input-bindings.ps1 -CleanPresentation
```

The opt-in fixture submits timestamped press/release edges through `UGameViewportClient::InputKey`, resolves the device to the local platform user, then waits for the normal input stack to invoke the existing pawn bindings. Pointer positions use the existing projected synthetic cursor seam. It never moves the physical cursor or injects OS input. The HUD/logs label fixture callbacks `Injected` / `inputSource=engineInjection`; a handled return alone is insufficient.

## Assertions and limits

- LMB selects and releases; held Shift adds/toggles exact IDs and releases. A preverified empty point clears selection.
- Ctrl+1/2/3 independently stores A+B, A and B. Three recalls of each require exact contents, then the original two-unit group is restored. Total group callback count is 17 after dead-member and post-rematch recalls.
- A mixed HQ/harvester/two-combat selection moves exactly its three mobile units; a subsequent enemy click attacks with exactly the two combat units. Require accepted server receipts, actual movement/damage and an unchanged HQ/harvester during attack. The normal bounded group queue and command sequencing remain active.
- A trusted fixture-only bot submits ordinary paid Factory/Produce/Attack commands through the shared host validation route. Ordinary combat kills one saved-group member; recall must retain the survivor. Its attack reaches a normal team victory. Native N starts epoch 2, clears pending selection/orders/groups despite reused IDs, and all three old groups recall empty.
- Wheel edges change and restore camera width. Arrow input must reach key-down state, then release. Positive pan uses an explicitly labeled **virtual-focus** stage seeded 10 cm from the ground limit and clamps there. `LostFocus` is explicitly simulated while arrow/Shift/Ctrl are held and must flush them.
- When the real observed focus/foreground predicate is false, assert suppression and record it. That conditional path was **not exercised in the passing run below** (`observedRealFocusSuppression=false`). Neither virtual focus nor simulated `LostFocus` establishes a real OS focus transition, mouse capture, Alt-Tab or hardware input.

The runner requires all 17 named acceptance checks, expected callback counts, 40+ injected edges, fresh 1280x720 PNGs with hashes, at least 10 identical shared epoch/revision digests across the dedicated server, rendered client and passive NullRHI observer, and a common epoch-2 snapshot. Host/client exit status and identity-checked cleanup are required. Bot decision suppression applies only to the explicit server fixture flag; normal game advance, idle fire, health, resources, costs and victory rules remain active.

## Tested source and retained failures

Tested clean source: `2de35c3a77b839c2d879599a6ee78f270b39515d`. The later documentation commit does not change compiled source. Build passed (4 actions, 41.04 seconds total / 39.66 executor). This is a bounded correctness test, not a representative performance measurement.

`generated/input-bindings-20261016-223258-992/result.json` passed: 174.378 seconds including a 22.767-second warmup; scenario 151.381 seconds within 180 seconds. All 17 required checks and 43 distinct client checks passed; 125 injected edges mapped device 0 to platform user 0. Callback totals: LMB 20, RMB 7, Produce 2, Build 2, Groups 17, WheelUp/Down 1 each, Rematch 1. All three participants agreed at 565 shared epoch/revision keys, including epoch 2; cleanup errors 0.

Four captures were inspected locally: `01-selected-group.png` shows both group highlights; `02-group-moved.png` and `03-group-attacked.png` show all four mixed-selection highlights and the combat movement; `04-terminal.png` shows Victory and the live selected survivor. These reuse the existing placeholder blockouts, do not approve visual scale, and remain ignored with logs/caches/binaries.

Retained failures, without rewriting evidence:

1. `input-bindings-20261016-222621-730` at `108cadf`: failed at stage 12 after 32 edges because the intended empty point overlapped a newly produced unit's selection radius. The fixture point was corrected and prechecked, and group assertions strengthened in `7f6ce38`. No gameplay rule was weakened.
2. `input-bindings-20261016-222946-927` at `7f6ce38`: failed the existing 60-second stage-zero readiness deadline (92.29 seconds total). No edges were injected. `2de35c3` adds exact readiness diagnostics; it does not claim to repair the unobserved cause. The following run observed all non-asset predicates ready, asset compilations decreasing to zero and successful readiness. A single later pass does not erase the intermittent startup failure.

`physicalInputVerified=false`, `osWindowFocusVerified=false`, `positiveRealFocusPanVerified=false` in every result. The positive pan focus source is `virtualFixture`; simulated focus loss is true only after that stage executes. A bounded human desktop check remains required before accepting physical Shift/Ctrl/mouse/pan/focus behavior.

## Resource and regression evidence

Each run uses a separate D: output/temp directory and explicit D: local cache, shared DDC disabled, hidden/offscreen rendering, a 30 FPS target, two-logical-CPU affinity and normal priority, bounded warmup/scenario deadlines, polling storage/CPU guards and owned-process cleanup. Affinity is assigned after startup and polling is not hard confinement. Warmup's CoreLimit setting is 4 while the shared affinity policy remains two logical CPUs; worker inheritance is not claimed verified. Inspect recorded paths rather than promising zero C: writes.

Regressions at the same clean `2de35c3` source all passed, serialized without overlapping engine jobs:

| Gate / generated run | Evidence |
| --- | --- |
| `fidelity-model-20261016-223606-672` | 4/4, 40.907 seconds |
| `native-model-20261016-223647-622` | 7/7, 12.078 seconds |
| `skirmish-model-20261016-223659-741` | 14/14, 12.549 seconds |
| `group-controls-model-20261016-223712-313` | 10/10, 12.513 seconds |
| `native-network-20261016-223724-898` | 61 matching revisions; runner does not record elapsed time |
| `rpc-ingress-20261016-223757-864` | Pass, 17.604 seconds |
| `controls-20261016-223815-506` | Pass, 58.607 seconds, 55 matching revisions |
| `small-match-coop-20261016-223914-147` | Pass, 95.255 seconds, 308 matching revisions, winner team 0; result capture inspected |
| `session-lifecycle-20261016-224049-443` | Pass, 120.390 seconds, 420 matching epoch/revision keys; two agreed terminal snapshots (both winner team 1), rematches to epochs 2/3, stale epoch rejected, four accepted fresh sequence-1 receipts, seat rejoin/full-slot checks; cleanup errors 0 |

The unchanged repeat `input-bindings-20261016-224300-394` also passed clean: 143.151 seconds total, 9.286-second warmup, 133.661-second scenario, the same 125 edges/43 checks/callback totals, 574 common epoch/revision digests including epoch 2 and cleanup errors 0. Its selected-group and terminal captures were inspected. Real-focus suppression again was not exercised. Neither passing run changed the fixed readiness or scenario deadlines; the earlier unexplained startup timeout remains a retained intermittent risk.

After all jobs exited, the same eight C: roots were compared with `generated/c-drive-before-input.json`: UE 5.7 Saved grew 5 bytes, D3DSCache grew 32,768 bytes, and the other six roots were unchanged. `generated/c-drive-after-input.json` records the observation. This shared-PC comparison is neither exhaustive nor exclusive attribution and does not establish zero C: writes. No fixture-owned UE/build process remained; `generated/resource-handoff.json` releases the heavy-test slot.

See [the supported-route plan](INPUT_AUTOMATION_PLAN.md), [group controls](GROUP_CONTROLS.md), and [dependency-aware integration recommendation](INTEGRATION_RECOMMENDATION.md). No branches are merged by this evidence.
