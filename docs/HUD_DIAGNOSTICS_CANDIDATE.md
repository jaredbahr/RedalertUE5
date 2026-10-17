# HUD and diagnostic integration candidate

This isolated branch combines only draft [PR #34](https://github.com/jaredbahr/RedalertUE5/pull/34) at `749c7e3f7e06779620b051766ec491cbf73fe3b6` and [PR #35](https://github.com/jaredbahr/RedalertUE5/pull/35) at `b093a82b1a7c9d04cdbf99d3c886f467fe56bc44`, based on main `0c7b07c21b6c794b83973f3735e8eb75dea49d9c`. Both source histories are preserved. [Manifest](hud_diagnostics_candidate.json) records exact code/runner identities and the pending command queue.

## Scope and review

- Three HUD text calls keep projected screen anchors independent of font scaling. Historical clean `8dc1f9f` passed one build and 1280x720 Victory capture inspection; dense labels still overlap.
- The fixture follow-up adds explicit default 1280x720 and optional 960x540 contracts. Actual readiness and PNG sizes must match. Source review and parameter-only checks passed; this new C++ follow-up has not been compiled or rendered.
- Bounded lifecycle phase telemetry preserves acceptance, simulation gates, sequencing, cleanup, resource policy and the total 240-second deadline. Independent review found no blockers. Historical clean `8f9dca3` passed one allocated run in 117.191 scenario seconds; no automatic retry or gameplay fix occurred.

No other feature branch is included. No label collision avoidance, new art, input fixture, instruction-file change, install or security/cost change is part of this candidate. The combined candidate is **unbuilt and untested**; historical passes must not be attributed to its current HEAD.

## Pending exact-candidate gate

Imagination owns the heavy-test slot during preparation. Wait for an explicit handoff. Reuse the existing integration checkout's ignored assets without generating content. Its cached module is currently from HUD source `8dc1f9f`; it cannot establish acceptance for this candidate. Prepare a clean new verification branch from the final published candidate commit, preserving all current worktrees/branches, then perform the four manifest commands serially: one build limited to one parallel action, one lifecycle run at 240 seconds, and one Victory run at each viewport at 240 seconds each. Record exact HEAD/clean state in every result.

Stop on build/runner failure or incomplete process cleanup; preserve it and investigate before requesting any further allocated run. No deadline extensions or automatic repeats. Keep normal command/economy/bot/terminal assertions; screenshots alone are insufficient. Inspect all four captures at native size using [HUD_VIEWPORT_CHECKS.md](HUD_VIEWPORT_CHECKS.md). Record narrow anchor results separately from dense ownership/HP readability and panel/edge limitations.

A successful narrow anchor regression check does not close [#18](https://github.com/jaredbahr/RedalertUE5/issues/18): existing dense overlap still needs a separate readability change and acceptance. The unexplained slow startup remains [#33](https://github.com/jaredbahr/RedalertUE5/issues/33), even if telemetry is integrated or the next run passes. Do not close #21-24 based solely on the latest diagnostic pass. Final art, physical input/focus, Internet behavior and combined workload budgets remain separate gates.

After missing checks and independent evidence review, recheck exact PR heads and current main before any routine integration decision. This draft candidate authorizes no merge by itself. No heavy job ran during its preparation.

Independent candidate review confirmed exact selected source/runner blobs and no unrelated features. Its only follow-up was a documentation clarification that MaxParallelActions=1 limits concurrency, not total build-action count; no code or acceptance changed after that review.
