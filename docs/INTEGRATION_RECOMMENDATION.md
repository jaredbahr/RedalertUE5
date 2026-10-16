# Integration recommendation

Recommendation only: no PR, branch or default branch was merged, retargeted or closed during this inspection. Integration needs Jared's explicit approval. The working source remains isolated on D:; generated assets, captures, caches and binaries remain ignored.

## Review and integration order

GitHub PR metadata and local ancestry were checked against main `2a41dadc72b7bb1153925b1949cfba510518dd4c`. The following is the chosen gameplay chain, not a suggestion to merge every open PR independently:

| Order | Draft PR / branch | Inspected head | Dependency / review focus |
| --- | --- | --- | --- |
| 1 | [#3](https://github.com/jaredbahr/RedalertUE5/pull/3) `feat/first-proof` | `68a5a76` | Based on main; original authored blockout pipeline and initial command experiment. Art approval remains separate. |
| 2 | [#9](https://github.com/jaredbahr/RedalertUE5/pull/9) `feat/ue-command-proof` | `462d2fa` | Based on #3; native authoritative command/replication foundation. Review with the corrections carried by #14. |
| 3 | [#14](https://github.com/jaredbahr/RedalertUE5/pull/14) `feat/integrated-controls-proof` | `4b07de6` | Based on #9; rendered controls and strict pursuit fixes, plus carried command-contract and asset-validation corrections. |
| 4 | [#19](https://github.com/jaredbahr/RedalertUE5/pull/19) `feat/local-interactive-slice` | `fd7eb1f` | Based on #14; bounded local launcher/input/HUD. Physical input acceptance is not implied by scripted handler tests. |
| 5 | [#25](https://github.com/jaredbahr/RedalertUE5/pull/25) `feat/small-skirmish` | `eb376db` | Based on #19; includes the ported owner-allocation and ingress protections before the small economy/bot loop. Review those protections here rather than merging their obsolete side branches wholesale. |
| 6 | [#26](https://github.com/jaredbahr/RedalertUE5/pull/26) `feat/session-lifecycle` | `4b9da35` | Based on #25; teardown, seat reuse, epochs, terminal/rematch and measured legacy model baseline. |
| 7 | [#27](https://github.com/jaredbahr/RedalertUE5/pull/27) `feat/group-controls` | `8f78dd0` | Based on the #26 source chain; bounded selection/groups and optional pan. The final #26 evidence-doc commits were carried as equivalent cherry-picks. |
| 8 | [#28](https://github.com/jaredbahr/RedalertUE5/pull/28) `feat/fidelity-baseline` | `0173ff8` | Based on #27; sourced baseline and stationary idle fire with retained fixture failure/repair. |
| 9 | [#29](https://github.com/jaredbahr/RedalertUE5/pull/29) `feat/input-binding-fixture` | Reviewed `9761db4`; historical runtime pass `2de35c3`; final bootstrap source `cad52da` builds but runtime verification remains blocked | Based on #28; later commits include diagnostics, a reproduced bootstrap-race fix and parser tests, not documentation alone. Preserve original unexplained stage-zero timeout, separate stage-16 failure and final pre-fixture startup timeouts. See [INPUT_BINDINGS.md](INPUT_BINDINGS.md). |
| 10 | [#30](https://github.com/jaredbahr/RedalertUE5/pull/30) `feat/nine-control-groups` | Clean tested `9bc6a4a`, source `c62e544`; build, 36 model cases and rendered handler compatibility pass | Also based directly on #28. Adds groups 4–9, local bounds/storage and one executed regression. Actual new-digit input remains unverified. No duplicate patches in #29; simulated combined tree is conflict-free, not built or accepted. The bounded independent batch passed without retries; reconcile with #29 only on an approved integration head and validate that exact resulting source before acceptance. |

Review the full chosen chain before integrating it; intermediate branches are historical partial proofs. Prefer retaining commits so stacked ancestry and tested source SHAs remain accessible. If a different merge strategy rewrites commits, reconcile each child's base and rerun the affected gates at the resulting source revision. Recheck target/head workflows and hooks before any approved publication; do not enable paid products, disable required checks or alter security settings to integrate.

## Side branches: carried changes versus unique work

| Draft / branch | Recommendation and evidence |
| --- | --- |
| [#8](https://github.com/jaredbahr/RedalertUE5/pull/8) `fix/command-contract-validation` (`ea1f9a3`) | Corrective prototype files are byte-identical to the copies in #14. The carried commit is `053bc7c`. Review the fix, then propose closing the old PR as superseded after the chosen chain lands; do not merge both histories blindly. |
| [#10](https://github.com/jaredbahr/RedalertUE5/pull/10) `fix/asset-evidence-validation` (`42d518f`) | The nine-file geometry/stale-evidence fix was reworked as `70193c5` in #14. Compared asset source files match. The old branch is not an ancestor; treat it as carried implementation, preserving unique historical evidence. |
| [#15](https://github.com/jaredbahr/RedalertUE5/pull/15) `fix/owner-allocation` (`9ef3770`) | Original head is **not an ancestor** of #25; adapted port `00f9f7c` **is**. Owner helper/test blobs match original, port and #25; runtime integration subsequently evolves in lifecycle. The original branch also contains final clean `6e2748e` 5/5 execution/resource/review evidence missing from the carried document's stale pending wording. [OWNER_ALLOCATION.md](OWNER_ALLOCATION.md) now preserves that final historical evidence and distinguishes current skirmish seats. This documentation gap is not a missing runtime fix or a reason to merge the old runtime wholesale. |
| [#20](https://github.com/jaredbahr/RedalertUE5/pull/20) `fix/rpc-ingress-budget` (`a7d41e6`) | Original head is **not an ancestor** of #25; port `3c7723f` **is**, followed by compile/link fixes and evidence. The two ingress helpers and focused test blobs match original, port and #25. Revalidate ingress at the integrated head; propose supersession after integration, without weakening the shared budget. |
| [#13](https://github.com/jaredbahr/RedalertUE5/pull/13) `test/asset-negative-probes` (`3057fd2`) | **Not wholly superseded.** It contains unique palette refresh, corrupted-FBX probes, export-validation requirements and stale-negative-report invalidation absent from the gameplay chain. Keep it open. Prepare a separate reconciled art-validation change on top of the selected integration head and rerun its asset gates before accepting that pipeline. Do not silently discard it or overwrite newer gameplay files with its older tree. |
| [#16](https://github.com/jaredbahr/RedalertUE5/pull/16) `docs/github-cost-guidance-20261016` (`d631272`) | Unique `AGENTS.md` contributor/fee guidance, based on #9. Review separately as instructions affecting future work. It was fetched/read for this recommendation and was not edited or copied into this branch. |
| `docs/project-foundation` (`eeb12d2`) | Historical planning branch without a PR in the inspected list. The chosen chain already carries and evolves the project brief/decisions/milestones; reconcile any unique wording if desired rather than replacing current documents wholesale. |

The final #26 documentation patches `694bc10` / `4b9da35` were carried into #27 as `d1275af` / `8b5ca74`; current `SESSION_LIFECYCLE.md` matches #26's final version. Reconcile this equivalent history if retargeting or rewriting the stack. Patch identity alone was not used to declare source equivalence: the owner/runtime ports adapt earlier context, and the asset-probe branch has demonstrably missing unique files.

## Issue mapping

These are draft-branch results; main remains the original README-only source. The issues below remained open when inspected. No issue was commented on or closed by this review.

| Issue | Evidence and remaining scope |
| --- | --- |
| [#11](https://github.com/jaredbahr/RedalertUE5/issues/11) identity/lifecycle | #15's allocator is carried, and #26 adds bounded cleanup/rejoin/epochs/rematch. Authentication, persistent identity restoration and Internet session behavior remain open; this is not an entirely missing allocator fix. |
| [#12](https://github.com/jaredbahr/RedalertUE5/issues/12) ingress | #20's bounded ingress implementation/tests are carried and loopback regression passes. Untrusted Internet CPU/network/log limits and adverse-network behavior remain unproven. |
| [#17](https://github.com/jaredbahr/RedalertUE5/issues/17) real input/focus | Historical #29 source proves injected binding dispatch for groups 1–3 and simulated focus cleanup; its later bootstrap fix is not runtime verified. #30's nine-group model regression now passes, but rendered compatibility uses direct handlers. Physical input/real focus, new-digit dispatch and the unexplained original startup timeout remain open. |
| [#18](https://github.com/jaredbahr/RedalertUE5/issues/18) HUD | Inspected 1280x720 captures show the current control/status/selection presentation. This does not close human readability, other viewport sizes or physical interaction acceptance. |
| [#21](https://github.com/jaredbahr/RedalertUE5/issues/21), [#22](https://github.com/jaredbahr/RedalertUE5/issues/22), [#23](https://github.com/jaredbahr/RedalertUE5/issues/23), [#24](https://github.com/jaredbahr/RedalertUE5/issues/24) small-match loop/economy/bot/result | The generic prototype has finite economy, shared-command bot actions, valid terminal outcomes and rematch evidence. Complete faction scope, calibrated original-game behavior, bot recovery/roles and declared combined performance/network budgets still require their own acceptance. |

## Gates before accepting an integrated result

Check exact resulting source, clean worktree and retained evidence; run the affected model/command/selection/fidelity tests, ingress and loopback consistency, input fixture with inspected captures, small-match Victory/Defeat/Coop and disconnect/rematch checks using the established bounded local runners. Do not substitute results from a different source hash. The separate art-validation lane requires its own geometry/negative-probe evidence. Preserve the distinction between injected input, simulated focus and actual physical desktop acceptance.

No merge, issue closure, release, asset finishing, binary/LFS upload or hosted workflow run follows automatically from this recommendation. Final visual scale, original gameplay calibration, Internet conditions and a practical army budget remain open gates in MILESTONES.md.
