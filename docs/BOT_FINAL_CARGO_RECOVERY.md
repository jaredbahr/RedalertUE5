# Bot final cargo with timed production

This composition starts from timed-production PR #71 at `22575d19c0f50805aeacf1d0b1d1b5f2d861d65d` and carries the cargo-recovery change from PR #74 at `40d912097b9c9a7faf34f6452e9af549cff6584d`. Their original branches, frozen inputs and validation receipts are unchanged.

After the last ore is exhausted, an explicit ReturnCargo canceled by refinery destruction can leave loaded miners without orders. The bot rebuilds a refinery and then returns cargo to the nearest committed live own refinery, breaking distance ties by unit ID. Ordinary Gather behavior, miner rotation, five-second retry cache, one-second decisions, shared command validation and four-command cap remain intact. The owner-specific active-production guard from timed production remains before Produce; it does not block miner recovery. Another owner's job does not suppress production.

## Fixture composition

The public recovery history now obtains its opposing attacker after the existing bounded harvesting loop has completed paid production. No separate pre-harvest time is inserted: loaded-cargo and remembered-order premises retain their original timeline. Recovery advances for the existing production duration and requires all jobs to finish before checking full accepted spending and subsequent exact cargo-to-credit conservation. Canceled orderless miners still cannot unload simply because a new refinery exists. Existing cargo, credits, total-gathered and playing-phase assertions remain required.

The planner case additionally checks cargo recovery and rotation while own production is active, held, cash-paused or fully paid awaiting release, plus isolation from another owner's job. These are snapshot-planner checks, not a claim that the explicit-return interruption was observed in an autonomous match.

A second public history copies the real rebuilt model before delivery, starts and holds a paid production job through accepted commands, and submits actual bot ReturnCargo intents. It resumes production after the first deposit, checking the credit ledger after every advance against gathered income minus actual production payments, then requires exactly one completed paid Combat and delivery of all original cargo. This covers held/resumed production concurrent with delivery; it does not establish a zero-cash production resumption case. The original production-free delivery oracle remains unchanged. Both histories retain the 60-step bound within the existing BotFinalCargoPublicRecovery case; no native name is added.

The native manifest requires exactly 71 distinct cases: the 62 common cases, all seven TimedProduction cases and both BotFinalCargo cases. The portable bot runner independently retains its eight exact groups. Timed-production model, fixture migrations, portable support files and policy scripts are inherited unchanged from PR #71.

## Validation status

Source composition and separate independent bot and fixture/manifest reviews completed without a blocking source finding. No compiler, functional test, optimized, Unreal, rendered or runtime acceptance result is claimed for this combined source. Static diff review and whitespace checks do not establish an executed gate.

Historical PR #74 implementation `56b09c25` compiled with MSVC C++17 /W4 /WX and produced eight passing groups, 497 functional checks and zero failures. Its enclosing owned-process stage FAILED after positive job activity following root exit. Forced cleanup confirmed zero active members; identity and cause remain unknown. Those results remain attributed only to their original inputs and do not validate this composition. PR #73's failed enclosing receipts and consumed diagnostic profiles also remain unchanged.

Keep this composition draft. Actual validation requires current resource admission and scheduling behind the primary game projects, unchanged owned-process and cleanup controls, all six timed-production policy gates before Unreal work, the complete native manifest and affected runtime acceptance. Do not rerun consumed diagnostics, increase deadlines or treat cleanup after a rejected stage as a passed gate. No runtime slot is reserved by this document.
