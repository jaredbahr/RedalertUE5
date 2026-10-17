# Cooperative context orders

## Problem and change

In cooperative skirmishes, right-click routing treated every different owner as an enemy. A teammate's unit therefore caused a rejected single-unit Attack, an empty group attack plan, or a rejected factory rally click even though the host already recognizes allied teams.

Single-unit, group, and factory right-click branches now share a snapshot relationship check with the attack planner. A live target owned by a known opponent on a different team is hostile in skirmishes. Friendly markers follow the existing ground Move, formation, or rally path. Hostile Attack routing and hostile factory-rally rejection remain in place. Harvester resource-click precedence is unchanged.

The check requires both participant records, positive finite target health, and known nonnegative teams. Legacy proof matches preserve distinct-owner hostility when both players have the same default team number. Match phase, context validation, and authoritative command validation remain in their existing paths. This does not change alliances, command formats, balance, or simulation ownership.

## Verification completed

- Root and independent reviewer compiled and passed all 28 cases against the production relationship header using installed MSVC with `/W4 /WX`.
- Independent review checked all three pawn branches, legacy behavior, unknown participants and teams, dead/nonfinite health, and resource-click precedence.
- PowerShell syntax and exact controls-test discovery checks passed: 12 unique names match the runner, with zero missing or duplicate names. `git diff --check` passed.
- Added native automation case `RedalertProof.Controls.CooperativeContextTargets` for the snapshot bridge and group plans. It has not been compiled or executed in Unreal.

The 28 compiled cases cover the relationship predicate. Source review covers the pawn call sites. Neither proves mouse routing or factory rally behavior at runtime. Previous branches' engine results do not validate this source change.

## Engine acceptance still required

Wait for the shared heavy allocation. Build this reviewed source, then run the exact 12-test controls suite with zero failures. Retain raw reports and bind them to the source commit and fresh binary.

Exercise real pawn routing in a cooperative fixture: single Combat and mixed Combat/Harvester selections right-click an ally marker and produce accepted Move orders; a selected factory right-clicks an ally marker and produces an accepted ground rally. Confirm a hostile marker still produces Combat Attack, a hostile marker still rejects factory rally, and a Harvester resource click still produces Gather. Check both participating clients and host receipts. Existing cooperative smoke tests alone do not cover these clicks; a targeted fixture or recorded manual acceptance is needed.

No Unreal build, runtime run, publication, or merge was performed for this follow-up. It is isolated from the held pointer-bounds and native Stop branches, based directly on main `f1a106ad8be722ce31bc4cc568c043b4b398d857`.
