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

Historical tested clean source: `2de35c3a77b839c2d879599a6ee78f270b39515d`. Build passed (4 actions, 41.04 seconds total / 39.66 executor). The newer bootstrap fix described below builds but still requires runtime validation; these earlier passes do not validate it. This is a bounded correctness test, not a representative performance measurement.

`generated/input-bindings-20261016-223258-992/result.json` passed: 174.378 seconds including a 22.767-second warmup; scenario 151.381 seconds within 180 seconds. All 17 required checks and 43 distinct client checks passed; 125 injected edges mapped device 0 to platform user 0. Callback totals: LMB 20, RMB 7, Produce 2, Build 2, Groups 17, WheelUp/Down 1 each, Rematch 1. All three participants agreed at 565 shared epoch/revision keys, including epoch 2; cleanup errors 0.

Four captures were inspected locally: `01-selected-group.png` shows both group highlights; `02-group-moved.png` and `03-group-attacked.png` show all four mixed-selection highlights and the combat movement; `04-terminal.png` shows Victory and the live selected survivor. These reuse the existing placeholder blockouts, do not approve visual scale, and remain ignored with logs/caches/binaries.

Retained failures, without rewriting evidence:

1. `input-bindings-20261016-222621-730` at `108cadf`: failed at stage 12 after 32 edges because the intended empty point overlapped a newly produced unit's selection radius. The fixture point was corrected and prechecked, and group assertions strengthened in `7f6ce38`. No gameplay rule was weakened.
2. `input-bindings-20261016-222946-927` at `7f6ce38`: failed the existing 60-second stage-zero readiness deadline (92.29 seconds total). No edges were injected. `2de35c3` adds exact readiness diagnostics; it does not claim to repair the unobserved cause. The following run observed all non-asset predicates ready, asset compilations decreasing to zero and successful readiness. A single later pass does not erase the intermittent startup failure.

`physicalInputVerified=false`, `osWindowFocusVerified=false`, `positiveRealFocusPanVerified=false` in every result. The positive pan focus source is `virtualFixture`; simulated focus loss is true only after that stage executes. A bounded human desktop check remains required before accepting physical Shift/Ctrl/mouse/pan/focus behavior.

## Follow-up startup diagnosis

Independent source/log review does not establish the failed run's concrete cause. It proves owner1/epoch1 snapshots continued, local input configuration occurred and zero edges were injected before stage0 expired. That build did not record every failed readiness predicate. Its shader-zero message occurs after exit was requested, so it cannot prove shaders caused the timeout. In the first later pass all logged non-asset gates were ready while total assets fell122→93→61→38→6→0; readiness occurred about13.6 seconds after the first snapshot. The unchanged repeat became ready about0.43 seconds after its first snapshot. Those are observations of passing startups, not a diagnosis of the earlier failure.

Installed `FAssetCompilingManager::GetNumRemainingAssets()` sums registered managers, and the normal engine loop already calls `ProcessAsyncTasks(true)` each frame. A total asset count cannot identify the blocked manager. No extra compilation pump, blocking finish, longer wait or relaxed acceptance condition is justified by this evidence. Preserve the failure as unexplained. Per-manager diagnostics and bounded repeat startup results are recorded below; the two passing runs above do not validate instrumentation added afterward.

Prepared local diagnostics sample on a monotonic one-second interval, force a final ready/timeout sample, and retain the same last sample in client JSON. Fields include owner/epoch/revision, live HQ/Harvester/enemy IDs, dimensions, configured input, all early prerequisites, remaining assets by registered manager, a failed-gate mask and time of the last predicate change. Ordinary snapshot revision changes do not reset that change time. The runner retains valid-prefix samples even if a final record is truncated and rejects malformed diagnostic evidence for acceptance.

Fifteen light PowerShell parser checks passed against retained logs and explicitly synthetic diagnostic records, including valid-prefix recovery and the input-bootstrap mask bit. They are not UE execution evidence. `-IsolatedCache` uses a fresh run directory before the unchanged warmup while retaining the engine read-only pak. No existing cache is cleared. Stage/client/scenario/warmup limits remain 60/175/180/300 seconds.

### Reproduced input-bootstrap race and pending fix validation

At clean `8cac8c7`, retained run `input-bindings-20261016-231245-284` established a separate stage-1 race: READY at frame 13 had no pawn input component, registration, R binding or completed input-stack frame. R press/release entered PlayerInput at frames 14/18; pawn binding setup occurred only at frame 23. No Build callback occurred before the unchanged stage deadline. Independent review confirmed injection preceded binding setup. This explains this reproduced stage-1 failure, not the original `7f6ce38` stage-zero failure.

`cad52da96091f5a1b77df271151a5a66f4cb7961` adds a shared readiness predicate requiring the current registered, enabled pawn input component, bound R delegate, usable viewport, and a completed normal input-stack processing frame containing that component. Diagnostics and the actual injection gate use the same predicate. The stack trace calls each normal engine implementation once; it is observational outside the explicit fixture. A completed stack frame alone does not prove a particular delegate ran; callback counters remain required. There is no fixed delay, replay, compilation pump or relaxed assertion. Exact selected IDs/kinds, hit ID and pending command state are also recorded, and mixed-selection assertions now require the intended IDs rather than counts alone.

The installed-toolchain build of `cad52da` passed: four actions, 12.50 seconds total / 8.97 executor. Independent source review found no blocking issue. Earlier compile failures are retained in `build-startup-diagnostics.log` (inherited Owner name collision) and `build-input-bootstrap-trace.log` (incorrect installed API assumptions); both were corrected before the successful builds.

Runtime validation of the final fix remains **blocked**:

| Clean source / retained run | Outcome |
| --- | --- |
| `701d8d8` / `230059-720` | Stage 1 timeout after early R edges; pre-trace bootstrap evidence incomplete. |
| `701d8d8` / `230351-953` | Fresh-directory pass: 150.381 seconds total, 16.885 warmup, 133.340 scenario; 125 edges, 43 checks, expected callbacks, 577 matching shared digests including epoch 2, cleanup errors 0. This predates the final gate. |
| `0ec82b3` / `230813-748` | Stage 16 timeout: two accepted move receipts instead of three. Bootstrap and R callback succeeded. Exact selected IDs were not recorded in this build; cause remains unattributed. |
| `8cac8c7` / `231245-284` | Concrete stage-1 bootstrap race above; retained failure. |
| `cad52da` / `231508-179` | Retained-cache warmup passed in 10.010 seconds; scenario exhausted its 180-second deadline before map/fixture execution (181.226 scenario, 191.466 total). No readiness samples or injected edges. Frame-zero platform configuration loads took 15.46–34.44 seconds individually. |
| `cad52da` / `231839-736` | Fresh-directory warmup exhausted its 300-second deadline (303.149 total); scenario never started. Frame-zero logs show material cache misses/compilation and continuing EnginePak shader-cache hits. No fixture samples or edges. |

Run identifiers in this table share prefix `generated/input-bindings-20261016-`; build logs and `startup-parser-checks.json` are also ignored local evidence. The two final-source failures establish prolonged engine startup work, not its underlying cause. They do not establish a blocked asset manager, exhausted memory/disk, or an OS/security fault. No limits or security settings were changed. The queued controls/native regression commands did not execute because the input runner failed first. A successful final-source input run and compatibility regressions remain required before acceptance; the draft branch is not ready to merge.

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

After the historical regression batch exited, the same eight C: roots were compared with `generated/c-drive-before-input.json`: UE 5.7 Saved grew 5 bytes, D3DSCache grew 32,768 bytes, and the other six roots were unchanged. `generated/c-drive-after-input.json` records that earlier observation, not a measurement of the later diagnostics runs. This shared-PC comparison is neither exhaustive nor exclusive attribution and does not establish zero C: writes. The current resource handoff records cleanup separately.

See [the supported-route plan](INPUT_AUTOMATION_PLAN.md), [group controls](GROUP_CONTROLS.md), and [dependency-aware integration recommendation](INTEGRATION_RECOMMENDATION.md). No branches are merged by this evidence.
