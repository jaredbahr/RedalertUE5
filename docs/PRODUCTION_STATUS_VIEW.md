# Production status text view

Source base: `22575d19c0f50805aeacf1d0b1d1b5f2d861d65d`.

This source-only candidate adds a pure read-only helper that formats the local owner's production job from one `FProofSnapshot`. It does not connect the text to the existing HUD. It adds no command, clock, state mutation, cache, timer, model field, network behavior, or input binding.

## Source contract

The helper is `ProofProductionStatusText::Local` in `unreal/Source/RedalertProof/Presentation/ProofProductionStatusText.h`. It accepts a const snapshot, local owner ID, and expected epoch. It returns no strings for a nonpositive or mismatched epoch, non-skirmish or non-Playing context, absent/ambiguous local player, negative local credits, duplicate local jobs, or invalid local job identity and ledger bounds. Other owners' jobs are ignored. In a valid context without a local job, it returns neutral full and compact idle labels.

The underlying job fields are `OwnerId`, `StartSequence`, `FactoryId`, `WorkMicroseconds`, `PaidCredits`, and `bHeld` (`ProofCommandModel.h:101-113`); the job list and snapshot epoch are replicated snapshot data (`ProofCommandModel.h:115-127`; `ProofGameState.h:14-15`). The helper uses the model's existing constants: 50 credits, 100,000 work microseconds per credit, and 5,000,000 total work microseconds (`ProofCommandModel.h:148-152`). These are the prototype's five simulated seconds, not measured RA2 timing or wall-clock ETA (`docs/TIMED_PRODUCTION.md:9,27`).

Progress is integer-floor `WorkMicroseconds * 100 / ProductionWorkMicroseconds`; paid credits and total cost are shown directly. The ledger bound is `WorkMicroseconds <= PaidCredits * ProductionWorkPerCredit`, evaluated in 64-bit arithmetic. Zero cash alone does not imply a stall: the already-paid credit quantum can fund additional work. “Waiting for credits” is used only when the current work has exhausted that paid allowance, work is incomplete, credits are zero, and not all credits have been paid.

## Display states

The state priority follows available snapshot facts. A matching live factory must have the local owner, factory kind, finite hit points, and positive hit points. If it is absent, the helper says “Factory unavailable” without claiming why it disappeared or when a refund will occur. Manual hold takes priority over completion. A complete job with a live factory says “Ready; waiting for release”; the snapshot does not identify whether capacity, unit IDs, or spawn placement is blocking release. Otherwise, exhausted paid work with no credits says “Waiting for credits”; remaining work says “Building”.

| Snapshot condition | Example status |
| --- | --- |
| Valid local context with no local job | `Production idle` / `Idle` |
| `WorkMicroseconds=1`, `PaidCredits=1`, local credits `0` | `Building` (prepaid work allowance remains) |
| `WorkMicroseconds=100000`, `PaidCredits=1`, local credits `0` | `Waiting for credits` |
| Complete job and `bHeld=true` with a live factory | `On hold` |
| Complete job with a live factory and not held | `Ready; waiting for release` (exact blocker unknown) |
| No matching live owned factory | `Factory unavailable` |

The work/cash examples assume an otherwise valid current-epoch local Playing snapshot, a live owned factory, and no hold unless a row says otherwise.

The status output contains state, factory ID, integer percent, and paid/total credits. It does not claim that a job will spawn, validate the snapshot as a security boundary, advance work, or estimate elapsed/remaining wall time.

## Integration and validation boundary

The existing HUD currently shows economy, input/receipt status, build controls, and selection but no production job status (`ProofHUD.cpp:169-187`). The existing timed-production instructions explicitly list progress UI and control bindings as gaps (`docs/TIMED_PRODUCTION.md:21,64`). These two new paths do not connect to the HUD or Pawn and do not amend any frozen acceptance binding.

This candidate is unbuilt, untested, and unadopted. It is not covered by PR72's existing build or by the separate HUD candidate. PR71's original acceptance gates remain unchanged. Future native tests, rendered integration checks, and any runtime acceptance remain pending and must follow the existing resource and execution limits. This small helper does not add string-mirroring tests or make a fidelity, timing, or performance claim.
