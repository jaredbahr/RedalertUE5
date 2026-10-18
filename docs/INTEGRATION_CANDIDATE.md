# Pinned skirmish and nine-group integration candidate

Draft preparation only. Main promotion requires Jared's later explicit approval after candidate verification. No issues are closed. The allocated combined queue passed once at clean 4fdc7e27f55bf38ea43314dbea58a40fc2f1afe2; all owned processes exited and the heavy-test slot is released. See [validation evidence](INTEGRATION_VALIDATION.md).

## Provenance and reconciliation

Branch `integrate/skirmish-nine-groups` starts at PR #30 `ab6bda4099cc85ca6d87d778fbb0b22fbf12b0a8`. Isolated reconciliation commit `65b2a73b24077b4e604abf93780a4a081809e797` also retains original PR #26 head `4b9da352e5612c778bcf0c9168ef20a6ac92d98e` as an ancestor. Its tree is byte-identical to #30: `380c7ac30a0d219baab11d2c03b70dbe4f82379e`. The final #26 documentation had already been carried equivalently into #27; `SESSION_LIFECYCLE.md` matches exactly. This reconciliation adds history, not source or gameplay changes.

The selected order is **#3 -> #9 -> #14 -> #19 -> #25 -> #26 -> #27 -> #28 -> #30**. Full pinned heads and the validation queue are in [candidate_provenance.json](candidate_provenance.json). Later candidate commits add only these documents and reconcile the historical allocator evidence; the final publication SHA is recorded in the draft PR and ignored local queue before execution. Every future result must record the actual clean tested HEAD.

Source tree `unreal/Source/RedalertProof` remains `9905f4c2f4cba7248ec48ab08029095ad24c442a`; the group-model runner (`unreal/Scripts/test-group-controls.ps1`) blob remains `0a874698c7ed15033588861a882372ed76156163`. No new project instructions, workflows or binaries were imported. The separate #16 contributor-instruction proposal is not applied. Existing ignore/storage rules remain intact.

PR #29 (`77073c5`) and #31 (`c3e5a16`) are excluded by ancestry and implementation: no input-binding fixture, higher-group fixture, bootstrap tracing or their runners are included. References to their separate historical results remain clearly labeled. Their unresolved failures are not silently accepted. Corrections from #8/#10/#15/#20 are already carried through the selected chain; do not merge those obsolete side histories wholesale. #13's unique asset probes and #16's instructions remain separate, open review work.

## Evidence boundary

Existing #30 clean `9bc6a4a` passed 36 model cases and rendered handler compatibility, with source hashes matching this candidate. Separate #31 clean `bff79ae` passed focused injected 4–9 save/recall/release/overwrite, but that combined fixture source is excluded here. These support selection of the candidate; neither is a new candidate run. The full #29 fixture's original readiness timeout, missing third move receipt and later pre-map failures remain unresolved. Do not classify them as harness-only without evidence.

Independent review verified the selected ancestry, exact tree/source identities, excluded fixtures and retained owner/ingress ports. The history-only reconciliation has no file delta. Six inherited blank lines at EOF appear in main-to-candidate whitespace checking; no source cleanup was introduced just to remove them. New documentation changes are checked separately. Review does not replace compilation or runtime verification.

## Recorded verification plan

The following plan was executed once at clean 4fdc7e2 with the pinned source/runner hashes, fourteen SHA256-verified reused assets and fresh per-gate result directories. The unchanged bounded runners were serialized; these retained reproduction instructions do not request another run:

1. Build once; then controls 11/11, native 7/7, skirmish 14/14 and fidelity 4/4.
2. Rendered controls with fresh inspected captures; native host/two-client consistency and ingress isolation.
3. Small-match Victory, Defeat and Coop; disconnect/rejoin/rematch lifecycle.

Stop on the first failed gate, preserve its exact phase/source/evidence and diagnose before proceeding. No automatic retry, timeout relaxation, startup retry matrix or overlapping heavy work. Verify owned-process exit and release the slot afterward. No art generation, installation, hosted workflow, binary upload, security change or zero-the system drive-writes promise is part of this queue.

The scoped candidate has passed this queue; request approval for its exact tested content before main promotion. Physical input/real focus, final art/scale, internet behavior and combined army-performance budgets remain separate acceptance gates. Issues #11/#12/#17/#18/#21–24 stay open; no release or milestone completion is implied.
