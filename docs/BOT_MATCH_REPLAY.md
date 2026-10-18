# Portable bot complete-match regression

This test compiles the real command model, bot planner, and terminal rules against
the existing `tests/portable_unreal` shim. It adds complete-loop coverage beyond
the native `BotModelPath` opening test: two opposing planners must build, harvest,
deposit, produce, fight, and finish. It makes no production-source changes.

## Run

From a Linux repository checkout with Bash and a C++17 GCC-compatible compiler:

```sh
bash tests/run_bot_match_replay_tests.sh
```

The default run builds ordinary and `-O2 -DNDEBUG` modes. Checks are explicit and
remain enabled under `NDEBUG`. Set `CXX` to select a compiler executable. The
runner uses a temporary directory and removes it on exit. It does not install
software or invoke Unreal, PowerShell, hosted workflows, or external services.

An optional separate AddressSanitizer/UndefinedBehaviorSanitizer build is:

```sh
bash tests/run_bot_match_replay_tests.sh --sanitized
```

Sanitizer support is required for that option. If the execution environment does
not support LeakSanitizer (for example, a ptrace-based sandbox), run it with
`ASAN_OPTIONS=detect_leaks=0` and explicitly report that leak checking was excluded.
Sanitizer diagnostics are fatal; do not count a compile failure as a passing test.

## Deterministic matrix

Each of six fixed setups runs twice from newly constructed model/planner objects
in one executable. Owners 1 and 100 use teams 0 and 1. Both planners receive the
same snapshot before each model step and submit through the ordinary trusted-owner
validator. There are no private setters, edited snapshots, free credits, scripted
combat orders, or altered planner parameters.

| Setup | First HQ | Second HQ | Submission order |
| --- | --- | --- | --- |
| Default seed positions, two planners | (-1400, -400, 0) | (1400, 0, 0) | 1, 100 |
| Default seed positions, reversed submissions | (-1400, -400, 0) | (1400, 0, 0) | 100, 1 |
| Horizontal symmetric | (-1400, 0, 0) | (1400, 0, 0) | 1, 100 |
| Vertical symmetric | (0, -1400, 0) | (0, 1400, 0) | 1, 100 |
| Short horizontal | (-900, 0, 0) | (900, 0, 0) | 1, 100 |
| Long diagonal | (-2200, -1200, 0) | (2200, 1200, 0) | 1, 100 |

The model receives `0.1f` per step; the planner's clock receives `tick / 10.0`.
The existing one-second decision cadence, four-command budget, costs, damage,
speed, and live-record cap remain unchanged. The 2,200-step observation bound is
220 nominal simulated seconds. Printed `simulated_seconds` is the nominal step
count divided by ten, not measured wall time or an engine timeout/performance
budget. This test does not modify any native or rendered acceptance time limit.

## Required assertions

- Both bots build a refinery and a factory; issue accepted Gather, Produce, and
  Attack commands; and actually receive mined-cargo credits.
- Every planner command is accepted for this bounded matrix, and no batch exceeds
  four commands.
- Every run reaches Finished within the observation bound. The winner must agree
  with surviving HQ ownership, including the simultaneous-draw case. A terminal
  flag alone is insufficient while both HQs remain alive.
- Every frame retains both owners, nonnegative economy/health, bounded cargo,
  finite world-bounded unit positions, and the live-record cap. Credits equal initial
  credits plus actual gathered credit minus accepted construction/production
  spending. Remaining ore, cargo (including cargo retained on dead records), and
  gathered credit are conserved. These assertions do not imply that every branch,
  such as a loaded-miner casualty, is reached by these six matches.
- Later Advance and rejected Submit calls preserve all terminal visible fields;
  both planners stop issuing commands.
- The second run's complete command/result and visible-state transcript exactly
  matches the first, including array order. The serializer covers every current
  field of `FProofSnapshot` and its records, `FProofCommand`, and
  `FProofCommandResult`. Update it if those structures gain visible fields. It
  does not rely on the portable `SecureHash` stub.

"Replay" here means fresh-run repeatability with both planners deciding again.
It is not playback of a recorded command stream, a network lockstep proof,
cross-compiler/platform determinism, or a promise that reversing submission order
must produce the same winner. No map-specific winner, finish time, production
count, or income amount is hard-coded as a balance expectation.

## Evidence and integration limits

The initial reviewed source base is commit
`4cacdd803220fb1ca1b6bc7a9cbc87ea686f04e8`. The source package records input Git
blob/SHA-256 identities, actual runner output, independent review, and isolated
deliberate-fault checks. Runtime source and the existing shim are dependencies,
not copies to add in this patch. The only additions are this document, the C++
test, and its shell runner. No native automation test names or expected-name
manifests change, because this is a standalone portable executable.

This is six reproducible fixture scenarios, not randomized/exhaustive gameplay
coverage. It is not Unreal compilation, rendered/physical-input/network acceptance,
performance evidence, pathfinding/collision behavior, or measured RA2/Yuri balance
and timing parity. Those existing acceptance and fidelity requirements remain.
