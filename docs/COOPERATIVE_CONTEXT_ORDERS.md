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
- Native case `RedalertProof.Controls.CooperativeContextTargets` is included in the runner's exact 12-test controls set.
- New evidence-policy synthetic tests: 59 passed for root and independent reviewer; final source/policy review passed with no blockers.
- Fresh Unreal build and runtime acceptance of this fixture: pending. Earlier branches' engine results do not validate these inputs.

## Allocated engine plan

After review and a fresh capacity check, bind the clean candidate commit, Source/Config/uproject trees, fresh DLL hash, build logs, and reused generated-asset manifest. Run the existing build helper, exact 12-test `test-group-controls.ps1`, default `test-controls-proof.ps1 -CleanPresentation`, then the opt-in cooperative command above. Require strict parser success, fresh PNGs, and independent raw-log and visual review before recording acceptance.

This branch starts at main `f1a106ad8be722ce31bc4cc568c043b4b398d857`. Pointer-bounds and native Stop publication holds remain in force. No publication or merge is authorized here.
