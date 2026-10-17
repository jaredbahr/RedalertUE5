# Stop validation

The bounded slice passed at clean b10e1ffb41ec4826e4e7755da292ac36c6c1af59. The [accepted batch ledger](evidence/stop-accepted-batch.json) records all 13 result files, raw log/report/capture hashes and compiled-module hash. Raw evidence remains locally ignored on D:.

| Gate | Result |
|---|---|
| Build | Passed, 17.44 seconds |
| Models | 62/62: Stop 11, skirmish 22, native 7, groups 11, fidelity 4, HUD 5, actual-model ownership recovery 2 |
| Stop controls | Passed, 44.453 scenario seconds; accepted Move5/Stop6/Move7; four fresh captures |
| Default controls | Passed, 42.876 scenario seconds; original assertions/two captures, Stop collection off |
| Victory / Defeat / Co-op | Passed, 84.023 / 60.175 / 106.589 scenario seconds; expected winners and every gameplay flag |
| Lifecycle | Passed, 126.871 scenario / 126.998 total seconds; unchanged 240-second budget |

All eight stationary common revisions 267 through 281 agree across host/rendered/observer on unit4 position (-756.504809,-470.179995) and cargo0, spanning 1.400000020 actual host-simulated seconds. Accepted Move7 then resumes movement toward (-700,-450). Immediate command before/after visible state is unchanged except its ordinary revision. All twelve fresh controls/skirmish PNGs were inspected by the lead and independent reviewer. Still images provide context; duration and exact state attribution come from logs and matching semantic snapshots.

Every required granted miner in Victory, Defeat and Co-op has matching identity, cargo50 and delivered50/empty-cargo checkpoints on all required roles. Lifecycle checks include terminal outcomes, two rematches, stale-epoch rejection, reseed, full seat, disconnect and rejoin.

The correction now uses a small policy helper called by the real Pawn and two UE tests. Ordinary model commands and fixed advances produce an owned/live source at send, actual death, and an ownership-rejected Attack4. The shared helper waits on the stale live snapshot, then removes only that source's issued-target mapping and clears the receipt latch on its exact owned-dead record. Model/snapshot digests stay unchanged; replay4 remains rejected, paid production5 and Attack6 remain accepted. Other rejection reasons, foreign/missing records, absent provenance and terminal states do not recover. This is actual model and shared-policy execution, not a reproduction of network timing.

None of this batch's three live matches encountered an ownership rejection. Their ownershipRetryEvidence arrays are empty; the existing dead-target retry is separate. The [earlier correction-only partial batch](evidence/stop-pre-recovery-coverage.json) at 57a5453 also passed 60 models and Victory/Co-op but left this coverage gap. That is why the deterministic actual-model tests were added, rather than repeating matches until the race occurred.

Jobs were serialized with unchanged deadlines, expected outcomes, command budgets and resource policies. All cleanup gates passed; a separate process check found zero engine/build/shader-worker processes before heavy-slot release. The 39 Stop-parser, 22 retry-policy and eight controls-policy synthetic cases are policy checks, not runtime or physical evidence. Native S counters remained zero. Physical input/focus, final art, Internet play and active army-scale acceptance remain open, as do issues #2, #17, #18 and #33. This lifecycle pass does not identify the earlier startup slowdown's cause.
## Retained first batch

The first batch stopped at its Victory failure. All evidence below belongs to clean e6728863a2c1f24e92c09eceff560444eec357fd. Later passing evidence does not reclassify this failure. [The retained ledger](evidence/stop-first-batch.json) records raw result/log/report/capture hashes.

| Gate | Result at e672886 |
|---|---|
| Build | Passed, 137.64 seconds |
| Stop model/selection | 11/11, 15.260 seconds |
| Existing models | Skirmish 22/22, native 7/7, groups 11/11, fidelity 4/4, HUD 5/5; 60 total passes |
| Opt-in Stop controls | Passed, 45.438 scenario seconds; original four requests plus Move5/Stop6/Move7; four fresh captures |
| Default controls | Passed, 42.268 scenario seconds; original four requests/two captures; Stop collection disabled |
| Victory | Failed, 50.912 scenario / 51.144 total seconds; ownership rejection before terminal result |
| Defeat / Coop / lifecycle | Not run; queue stopped at Victory |

The actual Stop run observed unit 4 at (-756.504809,-470.179995) across nine common host/rendered/observer revisions spanning 1.600000024 host-simulated seconds, then observed accepted Move7 returning it toward (-700,-450). Exact command before/after state, cargo and shared snapshot digests matched. All six controls captures were fresh and independently inspected. Native S counters stayed zero: this proves the shared handler/RPC path, not physical or native-key dispatch.

Victory's Attack96 addressed owned unit20 against target30. At 03:40:02.563 UTC, host/client revision623 showed unit20 owned by player1 with 2.5 HP. At 03:40:02.663 it was rejected for ownership. Exact validation-time HP was not logged. Death during the intervening advance is consistent with source and retained state, but remains an inference. The later missing-unit/terminal snapshot followed failure teardown and cannot establish pre-validation death. Stop was unused; no causal link to Stop is established.

## Correction boundaries

The fixture now captures the requested source ID and owned/live status before submission. Only an Attack/ownership rejection from that earlier owned/live source may wait for the same retained record to replicate as owned and dead. Missing or foreign records still fail. Recovery clears only that unit's issued-target entry and the fixture receipt latch, preserving the sequence allocator and every deadline/outcome assertion.

A bounded host diagnostic records actual unit existence, owner and HP synchronously after rejection, before any later advance. The offline gate requires every ownership retry to match that exact owned-dead host rejection and a later matching ordinary host/client revision. With the queued refinery evidence enabled, strict semantic validation also requires a prior positive owned client record and the exact playing-state dead record. Teardown, live/foreign/missing units, mismatched sequences and diagnostic overflow cannot justify recovery.

The correction retained the original failure and added strict host-death diagnostics plus 22 synthetic retry-policy cases. Its later exact-source execution and deterministic coverage are recorded above. No automatic retry, timeout increase or ownership relaxation was used.
