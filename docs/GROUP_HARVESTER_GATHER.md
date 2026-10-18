# Group harvester Gather

Prepared follow-up to the existing shared-command and ore economy work. An all-Harvester multiselection can right-click an ore patch to queue the existing Gather command for each eligible miner, in ascending unit-ID order. Enemy-unit targeting keeps precedence for groups; mixed selections keep existing Attack/Move behavior. A loaded miner may target a depleted patch to resume unloading; empty miners are omitted. No idle auto-gather is added.

Selection and queue recheck the current playing skirmish, participant, owned live Harvester, resource ID and own live refinery. Pending intents are pruned before the pawn stamps sequence/epoch. Stop and Move supersede pending Gather in the existing unit slot. Dispatch remains two commands per 0.1-second interval. The authoritative Submit path and all economy, rate and balance rules are unchanged. Resource picking retains the existing strict-inside 32-pixel radius and iteration order; the single-miner branch shares that picker.

Prepared validation (not compiled or executed locally):

- `unreal/Scripts/test-group-gather-model.ps1`: exact five `RedalertProof.GroupGather.*` native groups, original 90-second ceiling and identity-checked cleanup. Covers real starter/refinery-granted miners through planning, queue and authoritative Submit/Advance; selection/ownership/refinery/terminal/depleted-cargo boundaries; stale queue rechecks; canonical intents, pacing, atomic batches and explicit overrides.
- `tests/gather_eligibility_tests.cpp`: dependency-free production eligibility header, 292 explicit checks active under NDEBUG, including exhaustive prerequisite combinations and integer extrema. Intended normal and optimized C++17 runs on dot cloud; no Unreal simulation or rendering claim.

Engine build, native execution and pointer/runtime evidence remain pending fresh shared-host capacity admission. Existing group-control test membership is unchanged. Human trials and rollback are untouched.
