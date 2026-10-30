# Milestones

These are ordered gates, not scheduled dates or claims of completed work. Use placeholders until the visual gate approves finishing. Record evidence, hardware, build/configuration, test conditions, and unresolved failures for each technical gate.

## 0. Confirm setup and test budgets

Deliver: selected UE5 version, target hardware, an initial test range toward the large-army ambition (exact per-player and total unit budgets remain undetermined), hosting/connectivity choices for provisionally intended internet play and any LAN requirement, permanent designated non-system drive paths, cache verification plan, and proposed performance/network thresholds.

Pass: Jared confirms the unresolved choices; required tools are available without unapproved installs; source/generated-file rules and binary/LFS budget review are ready before content uploads. During the first separately authorized smoke run, record actual cache/temp/output paths and any system-drive use. Do not mark storage behavior verified from folder names alone.

## 1. Approve the visual blockout

Deliver: Tesla Reactor, Rhino Tank, and Conscript together, with normal and zoomed-out RTS-camera stills plus a motion view. See [Art direction](ART_DIRECTION.md).

Pass: Jared explicitly approves scale, proportions, silhouettes, and readability. Revise blockouts until approved; asset finishing starts only after this gate. Other technical proofs may use placeholders.

## 2. Prove commands and two-client authority

Deliver: a minimal test map with two connected player clients and host authority. Prototype the proposed shared human/bot command model using placeholders. Document whether the host is one of the player clients and the actual connection topology.

Pass: both players can select and command their owned units; move/attack and a minimal production/economy change agree on host and clients; invalid ownership/resource commands are rejected by the host; a scripted bot submits commands through the same validation path. Check spawn, damage, death, and match state on both clients. Record conditions and any divergence. This gate does not prove full skirmish AI or internet readiness.

## 3. Measure active-unit performance early

Deliver: a repeatable two-client workload combining active movement/navigation, combat, bot decisions, production activity, and network replication at the same time. Increase active counts through the agreed initial test range toward the large-army ambition; record per-player and total counts. Use measured results to set practical unit budgets rather than selecting an untested cap.

Pass: meet the thresholds agreed in milestone 0 on the declared hardware, with recorded frame-time statistics, host simulation/AI cost, pathfinding cost, memory, and network traffic/command latency. Include congested movement and simultaneous combat, not just idle units. Record the tested unit mix, map, session length, topology, and latency/loss conditions. Reduce scope or revise the design when limits fail; no performance pass exists without measurements.

## 4. Complete a small Allied/Soviet skirmish with bots

Deliver: one simple map and an agreed minimal roster supporting economy, base construction, production, movement, combat, defeat/victory, and restart. Include playable human-versus-bot and bot-versus-bot matches.

Pass: bots gather/spend resources, build a functional base, produce forces, attack, respond to threats, and recover from ordinary losses. Complete repeated matches to a valid victory/defeat with no blocking stalls or invalid resource/ownership behavior. Record seed/setup and match logs or captures. Recheck two-client consistency and milestone 3 budgets with the complete loop. A bot merely moving units does not pass.

## 5. Add Yuri and validate four players

Deliver: a scoped Yuri roster and its agreed defining mechanics, integrated into production, AI, combat, and networking; a four-player mixed human/bot test.

Pass: all three factions can complete matches; bots can operate each included faction; Yuri-specific state changes remain consistent and obey ownership rules on every participant. Four-player matches satisfy the agreed correctness and performance budgets. Record missing roster/mechanics explicitly rather than claiming full game parity.

## 6. Validate eight-player friend sessions

Deliver: sessions with up to eight player slots, a documented human/bot mix, measured per-player and total unit budgets, and connection instructions for the provisionally intended internet mode and any agreed LAN support.

Pass: complete repeatable eight-slot matches with consistent economy/combat/victory state and no blocking desynchronization, bot stall, or crash; meet the agreed active-unit and network budgets under the chosen conditions. Test connection failure/disconnect behavior and document the supported outcome. Validate actual internet conditions for the provisionally intended internet mode; LAN results alone cannot establish them. Eight human players remain unproven until tested with eight humans. Supporting eight large armies remains an ambition until the combined movement, combat, bot, and network-replication workload passes at that scale.

## Current engineering and future-goal order (2026-10-30)

The live [roadmap/dependency/claim index is issue #85](https://github.com/jaredbahr/RedalertUE5/issues/85).
The [design plan](DESIGN_PLAN.md) explains the incremental boundaries and acceptance
matrix. This section supersedes the older draft-stack ordering, not the numbered
product gates above or retained historical test evidence. This publication prepares
backlog only; no implementation, native job, formal approval or merge is requested.

Closed #21–24 retain their scoped generic base/economy/bot/terminal achievement.
Full faction fidelity, art approval, physical input, representative performance
and Internet gates remain open. Old open PRs are not automatically the current
integration path; always re-read current refs, comments and source evidence.

| Stage | Existing or new issue | Ready / blocked and exit |
| --- | --- | --- |
| A — reconcile current generic candidate | #17 input/focus, #18 HUD; PR71 → 75 → 76, independent77, sibling79/80 | Preserve exact open production/control/native/physical gates and coordinator reservations. #79/#80 are siblings on #76, not a tested combined chain. Any composition requires independent review and affected exact-head acceptance. |
| B — reference/rules/feasibility | #81 measured fidelity; #82 minimum Allied/Soviet contract; #2 combined workload | Ready to plan when the future goal starts. Measurements require permitted private references; roster commitments require owner acceptance; runtime requires declared budgets/hardware and a native allocation. |
| C — production player interaction | #83 | Blocked on applicable #71/#75/#76 production-control acceptance plus reviewed UI contract; coordinate shared Pawn/HUD changes with #17/#18/#72/#79/#80. |
| D — first role-specific loop | #84 | Blocked on accepted #81/#82 and #2 preliminary feasibility/protocol, plus mapped current acceptance. Remeasure representative role/assets afterward; don't create a circular final-scale prerequisite. |
| E — visual approval/pilot | #1; #6/#7 evidence gates | Existing blockout review first; finishing requires Jared approval, agreed subject/scale/zoom, rights and binary/LFS budget. Keep ART_DIRECTION and ASSET_PIPELINE_EVALUATION gates. |
| F — wider sessions/transport | #11/#12 | Existing local cleanup/epochs and controller ingress protection are partial successes. Wider design/implementation needs hosting/connectivity/identity choices, budgets and actual adverse-network acceptance. |
| G — expansion | Numbered milestones5–6 above | Yuri/four/eight players remain deferred behind the scoped Allied/Soviet loop and representative scale/Internet gates. |

Native acceptance is distinct from cloud/source review: exact installed Windows
UHT/MSVC, affected native suites, real two-client state/digest agreement, actual
rendered inspection and observed physical controls each retain their own scope.
Private diagnostic results do not automatically validate a public PR head. Keep
pending permissions, failed receipts and cleanup failures intact.

### Shared execution protocol for the later goal

Before edits, re-read repository instructions and current issue/PR comments,
resolve existing reservations, post a bounded GitHub claim on the work issue and
verify it exists. Include public worker/goal handle, UTC time, scope, branch/base,
files, dependencies, status and blocker; omit private local task/session IDs.
Re-read to detect races. Ambiguous/conflicting/denied claims block dependent
edits; never silently take over a stale claim.

GitHub comments are the shared source of truth, not private notes or an empty
assignee field. Native work separately needs the allocated heavy slot, fresh
unchanged resource admission and any app-control approval. Publish draft PRs,
report exact passed/failed/unrun gates, preserve failures, and close only the
accepted observable issue scope before releasing the claim/updating dependents.
The full protocol and current PR snapshot live in #85.

## Evidence discipline

The [first proof](FIRST_PROOF.md) supplies partial pipeline evidence, but no milestone is fully passed: art approval and the UE gameplay/network/bot/performance gates remain open. Record accepted results and deviations in [Decisions](DECISIONS.md); link future captures/logs without committing generated output or private information. Passing a small test does not establish final roster completeness, unlimited scale, or production readiness.
