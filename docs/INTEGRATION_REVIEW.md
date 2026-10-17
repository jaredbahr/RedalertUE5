# Nine-group integration review and validation queue

The original review performed no merge, retarget, main change, engine launch or compilation. After Imagination released the shared slot, the prepared build/model/rendered batch ran once at clean `9bc6a4a0562fedd28203cf4d1c684bdb65ccce40` and passed. All owned processes have exited and the slot is released. No feature expansion or source correction occurred. Subsequent draft PR #31 passed the focused combined-source injected-digit check at bff79ae; physical input/focus and broader integration acceptance remain open.

## Exact reviewed revisions

| Item | Revision / conclusion |
| --- | --- |
| Main | `2a41dadc72b7bb1153925b1949cfba510518dd4c`, unchanged |
| PR #28 base | `0173ff8d9004432857c9a012caf60ecd872e617a` |
| PR #30 reviewed head | `7c5cc8fb6f6658dc5b7d558afdf13d904622e598` |
| PR #30 implementation | `c62e544200bfca44c180d349009cb6b98a42de49`; the second commit maps documentation to the draft PR |
| PR #29 reviewed head | `9761db42ee753fb1f0f370d447cef1c0366062ac`; sibling of #30 from #28, not its dependency |
| Simulated #29 + #30 tree | `c93c7fe5157d3cdc259ba0db5378aa368ecfd9f6`; conflict-free `git merge-tree --write-tree`, no merge commit/worktree checkout, no build/runtime evidence |

`git cherry` reports both #30 commits as unique relative to #29. Both branches contain #28 once through ancestry; do not cherry-pick the entire base chain again. The simulated combined tree retains the new 4/9 bindings, #29 input-setup diagnostics and the eleven-test controls requirement. Conflict-free source merging does not establish semantic correctness.

The owner/ingress ports `00f9f7c` and `3c7723f` are ancestors of #30. Their original side-branch heads are not missing fixes to merge wholesale. Earlier review established equivalent command/asset corrections carried into #14 and the final #26 documentation carried into #27. PR #13's unique negative asset probes remain outside the gameplay chain, and PR #16's unique contributor instructions remain a separate review. Preserve their open status; see the [full side-branch recommendation](https://github.com/jaredbahr/RedalertUE5/blob/feat/input-binding-fixture/docs/INTEGRATION_RECOMMENDATION.md).

## Executed checks versus prepared tests

Executed at the initial review: independent source review, digit-to-callback static inspection, PowerShell runner syntax parsing, ancestry/patch inspection, simulated merge and `git diff --check`. No blocking source issue was found. UE execution had not yet occurred at that review.

Initially prepared, now executed: `RedalertProof.Controls.SelectionNineGroups`, with all nine independent saved contents, invalid 0/10, upper-slot death/foreign pruning and owner/epoch clearing. It and all ten existing controls tests passed. The same clean `9bc6a4a` also passed native 7/7, skirmish 14/14 and fidelity 4/4: 36 cases total. Build passed in 46.79 seconds; rendered compatibility passed in 63.586 seconds with 55 matching shared revisions, two inspected fresh captures and zero cleanup errors. No retry was required. See [exact run identifiers and limits](GROUP_CONTROLS.md). Historical three-group and #28 test results remain evidence only for their recorded earlier revisions.

PR #29's fifteen parser checks ran, and bootstrap source `cad52da` built. Its final-source runtime attempts failed before fixture execution. Historical input passes do not validate that fix. The original stage-zero readiness timeout and separate stage-16 move-receipt failure remain unresolved. Its input fixture injects only digits 1–3; even a future pass cannot establish #30's 4–9 dispatch. Do not repeat the inconclusive startup batch as part of this queue.

## Exact-commit plan and completed batch

The existing isolated D: worktree on `feat/nine-control-groups` ran steps 1–4 below once at clean `9bc6a4a`; both the recorded source tree `9905f4c2f4cba7248ec48ab08029095ad24c442a` and controls-runner blob `0a874698c7ed15033588861a882372ed76156163` matched before launch. Step 5 subsequently gained focused injected save/recall/release/overwrite evidence in [PR #31](https://github.com/jaredbahr/RedalertUE5/pull/31) at clean bff79ae; its human desktop checks remain pending. Later documentation-only commits may change HEAD; any source/runner difference requires refreshed review. Every result records the actual tested HEAD. These are retained reproduction instructions, not a request to repeat successful checks.

1. Obtain the shared slot handoff, then run `./unreal/Scripts/build-native-proof.ps1` once. Use its existing one-parallel-action/two-CPU policy and D: temp/log paths. A failure stops this queue for diagnosis; no automatic retry or timeout extension.
2. Run `./unreal/Scripts/test-group-controls.ps1` (existing 90-second bound); require the exact eleven cases, zero failures and successful exit. Preserve all failures and resource/cleanup evidence.
3. Run the existing native, skirmish and fidelity model runners serially: `test-native-model.ps1`, `test-skirmish-model.ps1`, `test-fidelity-model.ps1`. Require their existing exact 7/14/4 successes. These check unchanged authority/economy/idle-combat compatibility at the new compiled revision.
4. Before rendered compatibility, reuse only the fourteen previously validated ignored fixture assets with path/SHA256 verification. Those assets were copied and verified for this batch without generation or binary uploads. Run `./unreal/Scripts/test-controls-proof.ps1 -CleanPresentation` with unchanged bounds, inspect fresh HUD/selection captures and retain result/cleanup evidence. This is existing-handler compatibility, not new-digit input proof.
5. Keep actual 4–9/Ctrl binding dispatch as a separate bounded acceptance task. Check distinct groups 4 and 9, Ctrl release, empty assignment, death pruning and rematch clearing; distinguish engine injection from a human desktop check. No new fixture is claimed prepared by this review. Do not call #30 accepted until this gap has evidence.

Stop after the bounded batch, verify owned processes exited and release the slot. No binary/cache/capture uploads, installs, generated art, hosted workflows or OS/security changes are needed. These correctness checks establish no internet or army-performance budget.

## Merge recommendation and issue coverage

Retain the earlier chain **#3 → #9 → #14 → #19 → #25 → #26 → #27 → #28**, subject to full review and explicit merge approval. After #28, #29 and #30 are sibling additions: neither supersedes the other. #30's independent build/model/handler-compatibility batch passed; new-digit dispatch now has separate combined-source evidence in PR #31; physical-input acceptance remains pending. Preserve #29's pending status. Reconcile them only on an explicitly approved integration head, then build and run the affected model, input, loopback, ingress and lifecycle gates at that exact combined revision. The simulated tree alone is not a recommendation to merge now.

[#17](https://github.com/jaredbahr/RedalertUE5/issues/17) stays open: #30 adds familiar group slots, but new-digit dispatch is covered only by the separate combined-source focused PR #31 run; physical input and real focus remain unverified. [#18](https://github.com/jaredbahr/RedalertUE5/issues/18) stays open: the new build and inspected 1280x720 captures expose readable nine-group hints, but human readability and other viewport acceptance remain pending. Issues #11/#12 and #21–24 retain their broader lifecycle, internet, faction, bot-recovery and combined-workload acceptance gaps. No partial or unmerged issue is closed by this review.
