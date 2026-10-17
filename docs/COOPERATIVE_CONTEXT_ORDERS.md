# Cooperative context orders

## Player-visible fix

Cooperative right-click routing treated every different owner as an enemy. A teammate's marker caused a rejected single-unit Attack, an empty group attack plan, or a rejected factory rally click despite the host recognizing allied teams.

Single, group, and factory right-click branches now share a snapshot relationship check with the attack planner. Friendly markers follow existing ground Move, formation, or rally behavior. Hostile routing and resource-click precedence stay in their existing paths. Both participants, known teams, and positive finite target health are required to establish hostility; legacy distinct-owner opponents remain hostile with identical default team numbers.

## Focused acceptance fixture

`test-small-match.ps1 -Scenario Coop -CoopContextEvidence -CleanPresentation` opts into the existing dedicated host, rendered owner-1 client, and owner-2 observer. It enables existing refinery and rally evidence. The default runner remains unchanged when the flag is absent.

The fixture uses a projected synthetic cursor and actual `OrderUnderCursor()` calls. Selection setup is scripted; it does not inject physical mouse events or verify OS focus.

| Gate | Required result |
| --- | --- |
| Factory clicks hostile HQ | Synchronous rejection; no sequence allocation, queued intent, or snapshot change |
| Factory clicks allied HQ | Exactly one accepted SetRally; existing rally policy proves unchanged credits/IDs, published rally, paid production, spawn, and movement |
| Combat clicks allied HQ | Exactly one accepted Move and actual progress in at least two shared host/client/observer snapshots |
| Combat + Harvester clicks allied HQ | Exactly two accepted Moves; destinations preserve offsets from the action snapshot; both progress in shared snapshots |
| Combat clicks hostile HQ | Exactly one accepted Attack against the picked enemy |
| Combat + Harvester clicks hostile HQ | Exactly one accepted Combat Attack; no Harvester Attack |
| Harvester clicks ore | Exactly one accepted Gather, followed by receipt and completion hold |

Each marker action checks the exact picked ID and ground projection. The strict policy binds ten ordered phases, exact rendered snapshots, known teams, host sequences and destinations, and shared movement. The hostile factory and allied rally phases are synchronous. The single allied Move repeats the produced unit's rally destination; it proves routing and progress, without a direction-change claim. Two movement captures supplement the existing base, result, and rally captures.

The unchanged limits are two logical CPUs/Normal priority, one parallel build action at a time, 220 seconds of pawn scenario time, and 240 seconds of runner time. Existing soft cache/temp guards and owned-process cleanup remain in force. Failures retain their raw evidence; no unchanged functional retries or deadline increases.

## Verification status

- Production relationship predicate: 28 compiled MSVC cases passed for root and independent reviewer with `/W4 /WX`.
- Gameplay fix and initial fixture source review: no blocking findings.
- Fresh build at `8d11e03` passed in 49.8 seconds. All 12 native controls tests, including `CooperativeContextTargets`, passed. Default controls passed in 41.51 scenario seconds (58.897 total), with two fresh captures.
- New evidence-policy synthetic tests: 63 passed for root and independent reviewer; final source/policy review passed with no blockers.
- First cooperative run at `8d11e03` completed the game loop and ten context phases, but strict acceptance failed (`invalid command unit`). Its original failed receipt and raw logs are retained. Normal BuildRefinery/BuildFactory submissions use `unit=-1`; additionally, Attack receipts can precede the next published snapshot. The reviewed parser corrections passed 63 focused tests and the genuinely new bounded run described below. No earlier failure is relabeled as accepted.

## Verified engine acceptance

Build `8d11e03` passed in 49.8 seconds. All 12 native controls tests and default controls passed. The corrected policy at clean `41caaca` changed only scripts/documentation; Source tree `23dbfaedff28e4e1ea0c5d617275cc9803d14385`, Config and project inputs exactly matched the build. The bound DLL SHA256 is `49E134127E6092B21F7107AB927C2AD69CD9B3AB1ACFBAEA9D2F64FACA5036F2`.

The genuinely new cooperative run `generated/small-match-coop-20261017-151220-148/result.json` passed all three refinery, rally and context policies. Its receipt SHA256 is `5F5173A1EA2CCC264BE35529F7DF154DF692249239AD765BB92B0500AE58AD2B`. It records 305 common snapshots, ten context phases, 128 bounded owner-1 command observations, four common single-Move progress checkpoints and three common group-Move progress checkpoints. SetRally/Produce sequences 4/5, Move 6/7/8, Attack 9/10 and Gather 11 matched their gates.

Root and independent review verified all seven fresh native-size PNGs across default controls and cooperative acceptance, raw evidence, clean input bindings, and the retained first failure. These are synthetic-cursor shared-handler checks; physical mouse input and OS focus remain unverified. HUD issue #18 remains open.

The corrected receipt reports 270.804 scenario seconds and 271.147 total seconds because it includes slow startup and subsequent log-policy parsing. This is not a sub-240 total-duration claim. All three completion markers and final log-close markers occurred before the original process-loop deadline: latest completion 15:16:00.955 UTC and latest log-close 15:16:02.739 UTC, against roughly 15:16:20 UTC. The original process HasExited/zero-exit-code checks were retained. Per-process kernel exit timestamps were not separately persisted. Startup cause remains unproven.

The first `8d11e03` run remains a failed receipt (`B1E92BF9F3F0FD7211655D6967EC900981DAD81507CBEE4BA4DEAE81BC6A1D9B`). Corrected diagnostic parsing does not relabel it. Local root/independent reviews, build binding, corrected invocation, asset manifest and release receipt remain under ignored `generated/`. The heavy allocation was released at 15:17:16.2963695 UTC with zero UE/build processes.

This branch starts at main `f1a106ad8be722ce31bc4cc568c043b4b398d857`. Jared subsequently authorized routine publication of validated follow-ups. A combined candidate with pointer-bounds and native Stop needs its own applicable engine checks and independent review before readiness and merge; these standalone results do not validate that combination.
