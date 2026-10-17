# Stop validation: retained first batch

Integration is blocked by the first Victory failure. All evidence below belongs to clean e6728863a2c1f24e92c09eceff560444eec357fd, not the later prepared correction. [The retained ledger](evidence/stop-first-batch.json) records raw result/log/report/capture hashes.

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

## Prepared correction, not executed

The fixture now captures the requested source ID and owned/live status before submission. Only an Attack/ownership rejection from that earlier owned/live source may wait for the same retained record to replicate as owned and dead. Missing or foreign records still fail. Recovery clears only that unit's issued-target entry and the fixture receipt latch, preserving the sequence allocator and every deadline/outcome assertion.

A bounded host diagnostic records actual unit existence, owner and HP synchronously after rejection, before any later advance. The offline gate requires every ownership retry to match that exact owned-dead host rejection and a later matching ordinary host/client revision. With the queued refinery evidence enabled, strict semantic validation also requires a prior positive owned client record and the exact playing-state dead record. Teardown, live/foreign/missing units, mismatched sequences and diagnostic overflow cannot justify recovery.

Twenty-two synthetic retry-policy cases pass; independent source review found no static blocker. This does not prove actual recovery. The prepared correction needs a fresh exact-source build and the applicable model, Stop/default controls, Victory/Defeat/Coop and lifecycle gates before integration. No retry or timeout increase was performed.

All owned engine/build processes exited and the heavy slot was released for Imagination. Raw evidence remains ignored on D:. PR42 stays draft; issues #2, #17, #18 and #33 remain open.
