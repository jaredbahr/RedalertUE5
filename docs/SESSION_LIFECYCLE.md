# Small-match lifecycle and model stress baseline

This batch extends draft PR25's engineering foundation. It does not establish RA2/Yuri gameplay parity, a complete faction roster, Internet readiness or a practical army-size budget. Jared's direction is to preserve RA2/Yuri pacing, economy, recognizable roles and combat while improving the 3D presentation and modest camera/usability behavior.

## Supported local lifecycle

- The server binds each skirmish connection to one of two human slots, excluding bot owner 100. Commands use that exact controller binding; an old controller cannot command a reused slot. There is no account authentication or persistent identity restoration.
- Disconnect removes that participant's units, account, starter ore and stale orders. Other participants continue. A teammate carrying already-extracted ore keeps the return order and deposits it once even when the source patch disappears. If a team loses its last participant after both teams were represented, it forfeits; already-recorded results remain stable.
- Each human slot receives starting assets once per match epoch. A reconnect is a fresh connection to a vacant slot and waits without an army or starting grant until rematch. This prevents repeated disconnect/rejoin from minting credits and ore. First-time joins to an unused slot remain allowed while Playing. A third connection is rejected when both slots are bound.
- Active owner 1 can request a rematch only after a terminal outcome. The host advances the match epoch, rebuilds the initial model for connected slots and bot, and resets bot/round state. Queued commands from an older epoch are rejected before sequence admission. Clients ignore old receipts and reset their sequences/receipt history when the new epoch arrives. The existing RPC ingress budget/latch is shared by commands and rematch requests and is not reset by rematch.
- The camera pawn clears selection and unit views on epoch changes, preventing reused IDs from retaining a previous match's view. The terminal HUD shows the rematch key. Host migration, reclaiming an authenticated army, graceful lobby UX and Internet reconnect remain unsupported.

Prepared local playtest command (files and scripts verified; no visible game was opened for this batch):

```powershell
& 'D:\UserTemp\RedalertUE5-lifecycle\unreal\Scripts\start-small-match.ps1' -EngineRoot 'D:\Epic Games\UE_5.7' -SessionSeconds 900
```

Use the existing select/right-click/R/F/P/wheel controls. Owner1 presses **N** after victory/defeat/draw to request a rematch; **Escape** exits. The launcher gives a five-second warning and defaults to a 15-minute guard. Native physical input, focus behavior and the new N callback still require a human playtest; automated tests do not establish them. Prior manual game PID11560 and helper PID23448 were confirmed absent; their retained session ended normally at 17:59:21 UTC, not by timeout.

## Final validation

Validated compiled source: `649515a22b7822e180d8e7c7accaf6f23179b8e3`, clean at each listed final test's start. The fixture-only terminal evidence repair `aa208666d197ee03038d955f35248999e59556b4` was then rebuilt and verified at clean head `9f2d5626f4b40e1d2ae1ca935a625bd3d9dcedb9` after the resource handoff returned. That build succeeded in six actions / 8.39 seconds total (4.96 seconds in the parallel executor). No model or gameplay rules changed in that repair. Installed UE5.7.4 built the final cargo fix in five actions / 4.22 seconds. All jobs were serialized with two available logical CPUs, Normal-priority/polling policies, D: cache/temp paths and bounded test deadlines. Policy polling is not hard confinement. Result paths below are under ignored `D:/UserTemp/RedalertUE5-lifecycle/generated/`.

| Gate | Result | Evidence directory |
| --- | --- | --- |
| Skirmish economy/bot/rules/reset/disconnect groups | 14/14, zero failures, 12.253s | `skirmish-model-20261016-212446-725` |
| Existing native command/owner/ingress groups | 7/7, zero failures, 12.058s | `native-model-20261016-212459-004` |
| Active model stress baseline | Pass, three cohorts, repeated digest matches, 11.962s total editor wall time | `stress-baseline-20261016-212511-082` |
| Actual loopback lifecycle | Pass, 96.701s, 320 matching common epoch/revision digests | `session-lifecycle-20261016-212523-083` |
| Existing native host/two-client proof | Pass, 60 matching revisions | `native-network-20261016-212721-174` |
| RPC flood close and healthy-client isolation | Pass, 17.483s | `rpc-ingress-20261016-212752-291` |
| Offscreen rendered controls | Pass, 53.206s, 55 matching revisions | `controls-20261016-212809-839` |
| Existing cooperative rendered match | Failed overall, 90.739s; both clients passed and winner0, delayed host roster assertion failed after normal client cleanup | `small-match-coop-20261016-212903-083` |
| Terminal-roster fixture repair and Coop rerun | Pass, 91.058s, 332 matching common revisions, winner0, no cleanup errors; clean head `9f2d562` | `small-match-coop-20261016-214424-482` |

The lifecycle fixture uses a dedicated NullRHI host and sequential local client processes. Both initial clients receive matching state before owner 2 is terminated. The host cleans slot2, the rejoin binds slot2 without a grant, and a third connection is rejected. Epoch1 naturally ends in human defeat (winner1), epoch2 in human victory (winner0); both exact terminal snapshots agree across host and clients. Owner1 requests two rematches through the actual ingress-limited RPC, and both participants receive accepted sequence1 commands in epochs2 and3. A deliberately old epoch1 command is rejected in epoch2 without consuming its new sequence, and its receipt is ignored by the client. A requested finish ends the fixture after epoch3's fresh commands; shutdown forfeits are not an extra completed gameplay match. The scenario retains a 240-second deadline and verifies owner 1 stays connected before that finish gate.

The fixture pauses simulation while the runner arranges the roster/control gates. Combat outcomes use normal 0.1-second advances and ordinary build/gather/produce/attack commands, without direct health/resource grants or forced winners. These fixture gates are not a manual lobby feature. Model tests separately cover terminal removal, last-team forfeits, pending incoming/outgoing orders, surviving-player invariants and loaded foreign-ore delivery.

## Measured model baseline

Hardware context in `validation-host.json`: Ryzen 7 7800X3D, eight visible logical processors, approximately 63.2 GiB OS-visible RAM, Windows 11 Pro; the fixture used two logical CPUs at Normal priority on a shared PC. This is a legacy command-model workload, outside the small-skirmish cap, not a change to that provisional cap of 12 live owner records.

Each cohort has two owners and64 / 128 / 256 total Combat records with paired attack orders. It runs 100 advances of 0.1 simulated seconds, checks that every unit actually moved and took damage while remaining live, and repeats from a fresh model to verify the final digest. Per-step model advance is timed separately from snapshot copy plus canonical digest. The table reports median / p95 / maximum in milliseconds from the clean final run; startup and setup are outside those timers.

| Total units | Advance ms: median / p95 / max | Snapshot + digest ms: median / p95 / max |
| --- | --- | --- |
| 64 | 0.0026 / 0.0040 / 0.0103 | 0.0622 / 0.0815 / 0.5389 |
| 128 | 0.0090 / 0.0131 / 0.0210 | 0.1231 / 0.1383 / 0.2385 |
| 256 | 0.0329 / 0.0387 / 0.2692 | 0.2491 / 0.2670 / 0.3973 |

The whole editor process recorded 12.25 CPU seconds and a sampled peak working set of 2,259,042,304 bytes. This includes UE/editor overhead, not isolated army memory. There is no rendering, navigation/collision, bot planning, production or network replication in this workload. No FPS, eight-player result, latency/loss result or practical unit budget follows from it. The next scale gate must combine those costs in an actual rendered two-client workload.

## Retained attempts and repairs

- `build-session-lifecycle.log`: first build at `01f547f` failed (79.66s) because a fixture local `Player` hid the inherited controller member. The rename rebuilt at `531793e` successfully (4.45s). Review also replaced TArray capacity `.Max()` with the actual largest timing sample before the reported measurements.
- `native-model-20261016-211622-840`: startup identity check failed before tests (0.291s). A subsequent query found no owned engine process. The combined original check did not establish which identity field was unavailable; automation now retries only an empty executable path for up to two seconds, keeping the exact start-time/path guards. It fails explicitly if identity remains unverifiable. The successful native retry is retained separately.
- `session-lifecycle-20261016-211718-869`: failed at16.002s before disconnect because PowerShell treated a numeric epoch plus wildcard string as integer addition. Explicit string formatting fixed it.
- `session-lifecycle-20261016-211759-190`: all lifecycle gates completed, but the final assertion counted requested normal shutdown as an unexpected owner 1 disconnect (96.418s). The runner now checks the active window before the finish gate, parses UE log prefixes as UTC, and classifies later teardown separately. This report remains failed; it is not counted as a passing run.
- `session-lifecycle-20261016-212151-363`: passed at `1dd5f6f` (95.552s, 322 matching digests) before the final loaded-cargo teardown fix. Its following stress run passed but recorded a dirty source tree while that fix was being prepared; it is preliminary and superseded by the clean measurements above. No failed reports or captures were deleted or overwritten.

The retained cooperative failure at `649515a` reached human victory with every phase flag and both client completion checks passing. Observer and rendered clients exited normally before the host's five-second delayed assertion. Correct new Logout cleanup removed their player records, causing the legacy host roster check to fail. The reviewed repair latches the first terminal snapshot, clears it on rematch, and uses it only for that delayed evidence check. Live cleanup remains enabled; a match that never reaches terminal still fails on timeout. The clean rerun passed the unchanged cooperative phase flags, both client completion checks, winner0 and host delayed evidence check. Its victory/HUD capture was visually inspected; the earlier overall failure remains retained.
The native/controls/small-match parsers accept the new noncapturing epoch log field while keeping their prior digest comparisons. Multi-match comparisons explicitly key by epoch and revision, since revisions restart on rematch. Local binaries, caches, reports and captures remain ignored and are not uploaded as Git assets. Fourteen previously generated assets were reused with matching hashes; no new assets, installations or tools were introduced.

## Next bounded candidates

1. Camera panning plus responsive group selection/orders: bounded ground movement/zoom, clear multi-selection, and shared command validation without changing the RA2-style camera/readability goal; retain a human-input acceptance gap until actually played.
2. A reference-backed gameplay-feel baseline for economy/production and infantry-versus-vehicle roles: measure private original-game behavior before replacing instant placeholder construction/production and generic combat values. Do not upload proprietary assets or claim parity from names alone.
3. Combined active-scale proof: rendered client plus host/second client with simultaneous bot decisions, production, movement, combat and snapshot traffic; record frame times, simulation/AI cost, process memory and command/replication latency before setting a practical army budget.

Issue 11 remains broader than this local two-slot lifecycle: authentication, identity restoration, host migration and real Internet reconnect are still open. Existing cost, asset-rights, no-merge and security constraints remain. Newly filed issues should be checked again before the next batch.

At the resource handoff, the eight-root C: audit found UE5.7 Saved +18 bytes and the other seven monitored roots unchanged (`c-drive-before-lifecycle.json`, `c-drive-handoff-lifecycle.json`). This is a shared-PC observation, not exhaustive attribution or a zero-C-write promise. The controls captures and cooperative victory capture were visually inspected locally; binaries and PNGs remain on D: and ignored.


