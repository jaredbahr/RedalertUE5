# Refinery miner automatic first assignment

## Scope and reference

This is a new bounded economy feature, not a repair to the earlier refinery-grant contract. Building a refinery now also starts its newly granted Harvester on the existing Gather loop when ore remains. The already available BuildRefinery input and host command path reach the behavior; no new control, HUD, selection, command enum or replicated field is needed.

Greg Kasavin's contemporary, first-hand [Red Alert 2 review (GameSpot, October 27, 2000)](https://www.gamespot.com/reviews/command-and-conquer-red-alert-2-review/1900-2646216/) describes a new refinery's miner immediately heading for the nearest mineral patch. This is qualitative historical observation, not a publisher specification, local original-game measurement or proof of exact RA2/Yuri target selection. The publisher-authored [original RA2 manual, printed pages 16–17](https://manualmachine.com/gamespc/commandconquerredalert2/1117998-user-manual/) separately supports the gather/return loop and explicit miner orders; its inspected text does not establish the initial automatic assignment. Copies are third-party hosted; no proprietary material is redistributed here.

This follows the small-skirmish economy scope in [Milestones](MILESTONES.md). Production durations, construction queues, power, faction miners, navigation, fog and balance calibration remain separate work. All existing costs, speeds, cargo sizes, extraction/deposit rules and record limits remain unchanged. Starting harvest sooner intentionally changes economic trajectories; earlier match timings and outcomes must not be relabeled as results for this source.

## Authoritative contract

- Existing refinery placement, credit, two-record capacity, ID and spawn preflight still runs before mutation. One accepted build creates one refinery and one Harvester and charges one existing refinery price.
- Only the new miner receives one ordinary Gather order. Choose the nearest nonempty resource from its actual validated spawn, breaking equal squared distances by resource ID. Production resource records have positive IDs and finite bounded positions; resource ownership is teardown metadata, not a mining permission restriction.
- With no usable ore the build still succeeds, and the new miner stays idle. Later ore appearing does not wake an idle miner. Existing starter miners and prior manual orders are unchanged.
- Assignment is part of the accepted build, not an extra submitted command. It consumes no second sequence or command allowance and adds no revision beyond the existing build transaction. There is no immediate extraction or income.
- Ordinary positive Advance performs travel, finite extraction and own-live-refinery delivery. Existing depletion reselection remains in use. Missing assigned resources retain disconnect cancellation semantics. A missing refinery makes a loaded Gather wait for recovery as before.
- Explicit Move, Stop, Gather and ReturnCargo replace the automatic order by their existing rules. Completion of Move or Stop does not re-enable automatic harvesting. Death, owner removal, terminal freezing and reset retain existing order cleanup.
- Snapshot/digest schema is unchanged. Orders were already authoritative internal state rather than visible digest fields. Resulting positions, direction, cargo, ore and credits are observed through normal snapshots.

The shared ore policy now exposes its existing nearest-candidate scan for the initial assignment. Its existing SelectIndex entry point still returns no target for a missing prior assignment and retains a nonempty assigned patch. This must not become general idle harvesting.

## Focused test contract

Five native groups under RedalertProof.Skirmish.Economy.RefineryAutoGather:

1. StartAndConservation: real BuildRefinery only, no Gather; bounded travel/load/deposit; one-command budget and sequence/revision preservation; uninterrupted single- and multiple-granted-miner finite-ore conservation.
2. InitialTargetAndTie: distance from miner spawn rather than refinery; nearest over lower-ID-but-farther resource; both stable-ID tie directions; shared teammate ore.
3. ExplicitOverrides: valid foreign-owner rejection, preserved starter Move, own Stop/Move/Gather override, no post-Stop/Move restart.
4. NoOreAndReset: deplete real ore, successful no-ore build, no wake on later seed, owner removal and reset.
5. RefineryLossAndRecovery: actual enemy production/attack destroys the original refinery before deposit; carried ore waits and deposits after a rebuild without another Gather command.

The exact skirmish name manifest includes all five. Existing single-miner economy, ore reselection, Stop, ReturnCargo, removal and grant-preservation fixtures now explicitly Stop incidental grants through sequenced public Submit calls. Their prior exact income/conservation assertions are retained. The new feature tests never Stop the uninterrupted automatic miners. Existing multi-miner and bot tests remain active.

Run the committed focused portable runner with `ASAN_OPTIONS=detect_leaks=0 bash tests/run_refinery_auto_gather_tests.sh`. It compiles actual native test bodies and the actual model/rules, using the existing narrow Unreal shim and installed OpenSSL test-only MD5 bridge, in normal/O2/ASan+UBSan modes. This is not Unreal execution. No installation is needed; missing OpenSSL fails rather than substituting a digest. Leak detection is disabled in the cloud's ptrace environment, so no LeakSanitizer claim is made.

## Remaining integration acceptance

Portable source review and exact controls do not establish native or rendered acceptance. Before merging, require the installed UE build, exact expanded skirmish groups and relevant Stop/Gather/ReturnCargo regressions, plus the existing Victory/Defeat/co-op/lifecycle outcomes under their unchanged assertions, resource admissions and deadlines. Preserve any changed terminal trajectories and failed runs.

An automatic-start observer must identify the newly granted miner and correlate the accepted build, subsequent travel, load and own-refinery deposit with authoritative and receiving-client epoch/revision/digest state, while proving no intervening Gather command. The existing refinery-miner observer waits for an explicit Gather receipt; unchanged, it proves the old explicit-command path only. Update that evidence contract in a separately reviewed fixture rather than claiming it proves auto-start. Physical input, final art, Internet sessions and army-scale performance remain separate gates.
