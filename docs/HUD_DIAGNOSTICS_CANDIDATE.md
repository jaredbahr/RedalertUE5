# HUD and diagnostic integration candidate

This isolated branch combines only draft [PR #34](https://github.com/jaredbahr/RedalertUE5/pull/34) at `749c7e3f7e06779620b051766ec491cbf73fe3b6` and [PR #35](https://github.com/jaredbahr/RedalertUE5/pull/35) at `b093a82b1a7c9d04cdbf99d3c886f467fe56bc44`, based on main `0c7b07c21b6c794b83973f3735e8eb75dea49d9c`. Both source histories are preserved. [Manifest](hud_diagnostics_candidate.json) records exact code/runner identities and the executed command history.

## Scope and review

- Three HUD text calls keep projected screen anchors independent of font scaling. Historical clean `8dc1f9f` passed one build and 1280x720 Victory capture inspection; dense labels still overlap.
- The fixture follow-up adds explicit default 1280x720 and optional 960x540 contracts. Actual readiness and PNG sizes must match. Source review and parameter-only checks passed before the allocated build/render batch; the exact-candidate results are recorded in the validation document.
- Bounded lifecycle phase telemetry preserves acceptance, simulation gates, sequencing, cleanup, resource policy and the total 240-second deadline. Independent review found no blockers. Historical clean `8f9dca3` passed one allocated run in 117.191 scenario seconds; no automatic retry or gameplay fix occurred.

No other feature branch is included. No label collision avoidance, new art, input fixture, instruction-file change, install or security/cost change is part of this candidate. The combined candidate passed the exact prepared batch once at clean2470a582. See [current validation](HUD_DIAGNOSTICS_VALIDATION.md); historical passes remain separately attributed.

## Prepared exact-candidate gate (completed once)

Imagination owned the heavy-test slot during preparation. The completed batch waited for the explicit handoff; any further heavy run needs a new allocation. Reuse the existing integration checkout's ignored assets without generating content. Before the batch its cached module was from HUD source `8dc1f9f`; the batch rebuilt exact candidate `2470a582`, which is now the cached module source. The completed batch used a clean verification branch at 2470a582, preserved other worktrees/branches, and executed the four manifest commands serially: one build limited to one parallel action, one lifecycle run at 240 seconds, and one Victory run per viewport at 240 seconds each. Every result records the exact clean HEAD.

The executed protocol required stopping on any build/runner failure or incomplete cleanup. No such failure occurred in this batch; the older startup failure remains retained. No deadline extensions or automatic repeats. Keep normal command/economy/bot/terminal assertions; screenshots alone are insufficient. All four captures were inspected at native size using [HUD_VIEWPORT_CHECKS.md](HUD_VIEWPORT_CHECKS.md). Narrow anchor results were recorded separately from unresolved dense ownership/HP readability and unexercised marker-edge cases.

A successful narrow anchor regression check does not close [#18](https://github.com/jaredbahr/RedalertUE5/issues/18): existing dense overlap still needs a separate readability change and acceptance. The unexplained slow startup remains [#33](https://github.com/jaredbahr/RedalertUE5/issues/33), even if telemetry is integrated or the next run passes. Do not close #21-24 based solely on the latest diagnostic pass. Final art, physical input/focus, Internet behavior and combined workload budgets remain separate gates.

After the completed checks and independent evidence review, recheck exact PR heads and current main before any routine integration decision. Preparation itself authorized no merge. The later explicit routine-integration instruction applies only after the completed scoped gates and exact head/main verification. No heavy job ran during preparation; the allocated validation batch is recorded separately.

Independent candidate review confirmed exact selected source/runner blobs and no unrelated features. Its only follow-up was a documentation clarification that MaxParallelActions=1 limits concurrency, not total build-action count; no code or acceptance changed after that review.

The allocated batch completed at 2470a582; all owned processes exited and the slot was released. The independent full acceptance audit supports scoped integration and, after main verification, #21-24 closure using the complete evidence mapping. #18/#33 and unrelated gates remain open.
