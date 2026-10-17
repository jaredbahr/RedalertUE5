# Milestones

These are ordered gates, not scheduled dates or claims of completed work. Use placeholders until the visual gate approves finishing. Record evidence, hardware, build/configuration, test conditions, and unresolved failures for each technical gate.

## 0. Confirm setup and test budgets

Deliver: selected UE5 version, target hardware, an initial test range toward the large-army ambition (exact per-player and total unit budgets remain undetermined), hosting/connectivity choices for provisionally intended internet play and any LAN requirement, permanent D: paths, cache verification plan, and proposed performance/network thresholds.

Pass: Jared confirms the unresolved choices; required tools are available without unapproved installs; source/generated-file rules and binary/LFS budget review are ready before content uploads. During the first separately authorized smoke run, record actual cache/temp/output paths and any C: use. Do not mark storage behavior verified from folder names alone.

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

## Current engineering integration order

The native proof and small match are a foundation using generic units and placeholder values. The milestone pass criteria above still govern; the model-only stress result does not pass the combined active-scale gate.

1. Review the draft stack in dependency order: shared command/controls proof, ingress protection, small skirmish (PR25), local lifecycle (PR26), then the selection/groups branch. Integration requires explicit merge approval; none of these branches changes main. Keep issues 17/18 (input/HUD), 21-24 (small-match loop/economy/bot/result) and 11 (broader lifecycle) mapped to their actual evidence and outstanding gaps rather than closing them from a partial pass.
2. Finish the controls acceptance gap: a bounded rendered session must demonstrate multiple selection highlights, group recall and group Move/Attack through real host acknowledgements; a human must check Shift/Ctrl input and optional pan/focus behavior. Keep the default camera and existing single-unit actions as compatibility gates. See [group controls](GROUP_CONTROLS.md).
3. Establish a private original-game reference baseline for economy, construction/production timing, movement, range and infantry/vehicle roles. Record observed conditions and agreed tolerances before replacing placeholder rules; validate the revised loop and bots through shared commands. Do not upload proprietary assets or infer parity from unit names.
4. Run the combined rendered two-client active workload from milestone 3, including bots, production, movement, combat and replication together. Measure frame times, simulation/AI cost, memory and command/network latency before choosing practical army limits. Then complete the scoped Allied/Soviet roster and repeat bot matches before Yuri and four/eight players.

No paid infrastructure, hosted workflow runs, roster expansion or new asset generation is needed to review the current branch. Internet conditions, authenticated reconnect, final unit scale, visual approval and practical army budgets remain separate open gates.

The [fidelity baseline](FIDELITY_BASELINE.md) now records manual-supported behavior separately from prototype values and exact idle-fire test evidence. Production/economy calibration remains the next reference-measurement task. The [engine input automation plan](INPUT_AUTOMATION_PLAN.md) defines a synthetic binding test for the controls gap without claiming real desktop focus or hardware input.

## Evidence discipline

The [first proof](FIRST_PROOF.md) supplies partial pipeline evidence, but no milestone is fully passed: art approval and the UE gameplay/network/bot/performance gates remain open. Record accepted results and deviations in [Decisions](DECISIONS.md); link future captures/logs without committing generated output or private information. Passing a small test does not establish final roster completeness, unlimited scale, or production readiness.
