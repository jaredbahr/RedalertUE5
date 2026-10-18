# Group harvester Gather

Prepared follow-up to the existing shared-command and ore economy work. An all-Harvester multiselection can right-click an ore patch to queue the existing Gather command for each eligible miner, in ascending unit-ID order. Enemy-unit targeting keeps precedence for groups; mixed selections keep existing Attack/Move behavior. A loaded miner may target a depleted patch to resume unloading; empty miners are omitted. No idle auto-gather is added.

Selection and queue recheck the current playing skirmish, participant, owned live Harvester, resource ID and own live refinery. Pending intents are pruned before the pawn stamps sequence/epoch. Stop and Move supersede pending Gather in the existing unit slot. Dispatch remains two commands per 0.1-second interval. The authoritative Submit path and all economy, rate and balance rules are unchanged. Resource picking retains the existing strict-inside 32-pixel radius and iteration order; the single-miner branch shares that picker.

Validation preparation and checkpoint scope:

- `unreal/Scripts/test-group-gather-model.ps1`: exact five `RedalertProof.GroupGather.*` native groups, original 90-second ceiling and identity-checked cleanup. Covers real starter/refinery-granted miners through planning, queue and authoritative Submit/Advance; selection/ownership/refinery/terminal/depleted-cargo boundaries; stale queue rechecks; canonical intents, pacing, atomic batches and explicit overrides.
- `tests/gather_eligibility_tests.cpp`: dependency-free production eligibility header, 292 explicit checks active under NDEBUG, including exhaustive prerequisite combinations and integer extrema. Intended normal and optimized C++17 runs on dot cloud; no Unreal simulation or rendering claim.

Gameplay checkpoint `58b511c` passed its build and all five focused native groups. The actual portable header tests passed all 292 checks normally and optimized on dot cloud. Those results remain bound to that source checkpoint; the opt-in instrumentation below still needs its own build/native/runtime validation after fresh shared-host capacity admission. Existing group-control test membership is unchanged. Human trials and rollback are untouched.

## Opt-in pointer-to-dispatch proof

`test-small-match.ps1 -Scenario Victory -GroupGatherEvidence` adds a bounded fixture to the existing natural Victory run. It enables the existing refinery semantic evidence, selects the real starter with `SelectUnderCursor`, adds the refinery-granted miner with the existing selection toggle, and calls `OrderUnderCursor` once over projected ore. It checks two unstamped queued intents, normal Tick dispatch, two consecutive accepted receipts, and actual movement, cargo and deposit progress for both miners. One additional `03-group-gather.png` capture uses the existing PNG dimension/hash/freshness pipeline. The original 240-second runner ceiling, 220-second scenario deadline and bot/economy/Victory assertions remain unchanged. Other evidence combinations are rejected for this opt-in fixture.

The companion parser binds four explicit phases to validated rendered snapshots, both accepted authoritative commands to shared Request records, starter/granted provenance, and both miners' common-state movement/load/deposit transitions. Its synthetic parser fixtures exercise fail-closed malformed, missing, duplicate and contradictory inputs. Passing parser fixtures is not engine execution. This new instrumentation requires its own reviewed build/native/runtime checkpoint; earlier source acceptance does not automatically cover it.
