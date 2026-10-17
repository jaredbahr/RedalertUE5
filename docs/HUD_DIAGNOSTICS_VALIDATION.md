# HUD diagnostics candidate validation

All four prepared commands passed once at clean **2470a5821d6dabcb9d27cd79720a6bc21f466630**. No failed attempt, source fix during the batch, automatic retry or deadline extension occurred. Independent result/capture review found no blocker to the scoped integration. Publication changes after that revision are documentation/evidence only.

| Gate | Retained run under ignored generated/ | Result |
|---|---|---|
| Build | hud-diagnostics-build.log | Four actions, one parallel action, 13.10s total / 9.18 executor |
| Lifecycle | session-lifecycle-20261017-012035-526 | 120.600s scenario / 120.711 total, fixed 240s, 75 events, zero drops, 421 shared digests |
| Victory 1280x720 | small-match-victory-20261017-012236-307 | 66.665s scenario / 66.818 total, fixed 240s, actual winner 0, all six loop flags |
| Victory 960x540 | small-match-victory-20261017-012343-136 | 64.319s scenario / 64.435 total, fixed 240s, actual winner 0, all six loop flags |

Lifecycle verified two exact terminal agreements (both winner 1), rematches into epochs 2/3, all four accepted fresh sequence 1 receipts, slot 2 disconnect/rejoin/deferred reseed, full-session rejection and stale-epoch rejection. Play1 began at 34.111 monotonic seconds, leaving 205.889; play2 began 73.926, leaving 166.074. This passing run does not explain or erase the prior main startup failure retained in [the timing review](LIFECYCLE_TIMING_REVIEW.md). [Issue #33](https://github.com/jaredbahr/RedalertUE5/issues/33) remains open.

All four fresh PNGs were inspected at native size by root and an independent reviewer. Actual viewport/PNG dimensions matched 1280x720 and 960x540. Unit labels follow markers rather than scaling toward the origin; ore labels stay adjacent to crosses; objective/control/cost/status and terminal/rematch panels remain contained. Dense clusters visibly overlap labels at both sizes. This is a **narrow anchor/viewport regression pass**, not complete dense readability or arbitrary marker-edge acceptance. [Issue #18](https://github.com/jaredbahr/RedalertUE5/issues/18) remains open. Existing CleanPresentation overlay suppression was explicit; diagnostics remain in logs. Captures remain ignored locally, with hashes in the [curated result and phase ledger](evidence/hud-diagnostics-candidate-20261017.json).

## Full bounded-milestone acceptance audit

The following mapping uses the full retained evidence, not the latest lifecycle pass alone. Commands, Skirmish, Runtime and Tests are byte-identical from fully tested 4fdc7e2 through 2470a582. The only engine changes in this candidate are the three HUD anchor flags and explicit viewport fixture setup/readiness. The prior [integration validation](INTEGRATION_VALIDATION.md) retains 36 successful model tests, actual Victory0/Defeat1/Coop0 (307 shared revisions), native networking, rendered controls and ingress regression passes. Fresh main 0c7b07c also passed all 36 model cases before this candidate.

| Issue | Complete scoped evidence |
|---|---|
| #21 small base-to-victory milestone | Economy/terminal models; retained actual Victory/Defeat/Coop two-client command/snapshot checks; preserved native/controls/ingress; current exact-candidate build/lifecycle/two viewport regressions |
| #22 authoritative finite economy | Ownership/prerequisite/cost/capacity/finite-nonnegative-resource/digest/simultaneous-combat tests; real harvest travel/cargo/deposit and factory progression in retained and current matches |
| #23 command-driven bounded bot | Deterministic cadence/monotonic sequence/legal placement/target/terminal/progression tests; actual bot refinery/factory, production and attack flags through the shared route; bot gathering is supported by the model/progression tests, not a separate runtime gather flag |
| #24 terminal rules and basic match HUD | HQ survival/draw/stable-terminal tests and retained Victory/Defeat/Coop; inspected objective, controls/costs, role/cargo/credits/ore and clear terminal/rematch text; broader dense readability stays #18 |

Independent acceptance review supports closing #21-24 only after the verified source reaches main and the exact main/tree guard succeeds. No physical-input/Alt-Tab, final-art/scale, Internet, persistent-session or active-army performance gate is included in those closures.

## Handoff

All runner cleanup-error arrays are empty, owned UE processes exited, and the shared heavy slot was released before publication/integration work. The existing integration checkout's compiled module now corresponds to 2470a582, source tree fd8353a754ff4cd447d074509018794b93f6f580. Build/temp/cache and asset reuse retained the existing D: paths and resource policy; no installs, asset generation, binary/capture uploads or zero-C-writes claim occurred.

Verify the final publication delta is documentation only, recheck exact candidate/PR heads and current main, then use the routine authorized integration guard. Main verification must show the merge's source tree and both runner blobs match this tested candidate; no source-changing merge is accepted by this evidence.
