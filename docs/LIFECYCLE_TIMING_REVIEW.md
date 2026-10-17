# Lifecycle timing diagnosis and next check

Main `0c7b07c21b6c794b83973f3735e8eb75dea49d9c` has an unresolved fresh lifecycle failure. Its game source and full merged tree match approved candidate `5b90e8c`; game source also matches tested candidate `4fdc7e2`. The earlier pass does not clear the new failure. Issues #21-24 remain open pending verification.

## Retained evidence

Both runs are under `D:\UserTemp\RedalertUE5-integration-candidate\generated`. Use result JSON process `startUtc`, engine log timestamps and retained control-file write timestamps. Directory names are creation labels, not the scenario clock start.

| Interval, wall seconds | Candidate `session-lifecycle-20261017-002102-766` | Main `session-lifecycle-20261017-004312-990` |
|---|---:|---:|
| Host process start to ready | 6.553 | 48.023 |
| Lead process start to owner 1 join | 6.646 | 27.735 |
| Owner 2 process start to join | 7.282 | 30.584 |
| Rejoin process start to join | 7.074 | 29.426 |
| Recorded result start to play epoch 1 gate | 37.353 | 180.801 |
| Play epoch 1 gate to terminal 1 | 38.799 | 38.806 |
| Play epoch 2 gate to terminal 2 | 44.493 | Not reached |

The failed result records start `00:43:15.5730397Z`; play epoch 1 was written at `00:46:16.3739847Z`; terminal 1 arrived `00:46:55.180Z`; play epoch 2 was written at `00:46:56.3922460Z`. Approximately 19.2 seconds remained in the fixed 240-second scenario budget. It timed out waiting for epoch 2 terminal: scenario 240.023 seconds, total runner 242.618 seconds, cleanup errors empty. The passing run completed both terminals and epoch 3 checks in 123.808 scenario seconds.

The runner starts one monotonic scenario clock before launching the host, then launches four clients serially, including replacement and rejected clients. Startup, joins, snapshot checks, both matches and shutdown share that budget. The game holds simulation until each `play-epoch-N` file exists, then advances fixed 0.1-second steps using accumulated engine delta with bounded catch-up. Startup time does not give bots a simulation head start.

The first battle progresses at effectively the same wall time in both runs. Slow pre-play startup consumes the failed run's budget. Logs place substantial delay before engine/map readiness across the host and clients; they do not establish whether scheduling, I/O, caches or another startup subsystem caused it. No OS/security cause, performance benchmark or second-battle correctness is established. Epoch 2 accepted commands exist in client logs; the final `freshReceiptSequences` array is populated late, so its empty failed-run value is not evidence that those receipts failed.

## Diagnostic-only change

`test-session-lifecycle.ps1` now records a bounded history of monotonic phase observations: role launch request, process start, marker observation with engine log UTC separately, completed gate write and failure context. UTC fields correlate logs; elapsed/remaining-budget fields use the existing Stopwatch. History is capped at 256 events and records overflow. No gameplay code, launch settings, resource policy, sequencing, acceptance assertion, retry or timeout is changed.

Completed offline checks: PowerShell syntax; fractional monotonic elapsed/remaining budget; engine timestamp correlation; active wait context; existing timeout at 241 seconds; bounded history/overflow with current phase retained; completed gate writes; JSON serialization. A PowerShell integer-overload rounding defect in the new diagnostic field was caught and corrected before commit. These helper checks used mock logs/processes and launched no engine. They do not verify Unreal lifecycle behavior.

## One queued next run

Imagination currently owns the heavy-test slot. Do not start this check until the slot is explicitly handed back. Preserve both original runs; do not automatically retry or extend the deadline.

When allocated, prepare a new local verification branch from this diagnostic commit in the existing integration checkout, preserving its current branch and ignored map/assets/binaries/cache. Confirm a clean tree and unchanged `unreal/Source` against main. Run exactly once with the existing defaults:

    .\unreal\Scripts\test-session-lifecycle.ps1 -TimeoutSeconds 240

Record exact HEAD, clean state, phase events and existing acceptance results. Compare startup requests/process starts/engine initialization/joins, gate-to-terminal durations and time remaining at each play gate. All existing two-terminal, rematch-to-epoch-3, stale-epoch, fresh-sequence, owner/slot, matching-snapshot, full-session rejection and cleanup checks must pass within the unchanged total budget. A startup-only or model-only result cannot pass this gate. Another failure remains a failure and should identify the next source/log investigation; do not increase the timeout to fit it.
