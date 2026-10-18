# Native command proof acceptance

This is a bounded correctness fixture, separate from the Node contract prototype.
It uses original code and placeholder scene assets. It does not establish internet
connectivity, eight-player readiness, army budgets, or performance.

## Native automation

After building the editor module, run `unreal/Scripts/test-native-model.ps1`.
It runs `Automation RunTests RedalertProof.Commands` in UE 5.7.4
`UnrealEditor-Cmd.exe` with `-unattended -nullrhi`, four-core limit and a maximum
90-second deadline. Fresh logs, report, cache and temporary files stay on the designated non-system drive.
It requires the JSON report to contain exactly the four named tests with state
`Success`, four successes and zero failures, in addition to a clean exit code.

- Validation: identical human/bot command route, ownership, trusted player,
  replay, malformed enums and nonfinite positions, bounds and friendly targets.
- Resources/rate: production cannot overspend; per-advance allowance resets.
- Simulation: mutual lethal damage, bounded movement, detached snapshots and
  deterministic visible-state digests.
- Phase order: reversed unit insertion cannot bias movement or mutual damage;
  all movement reads the old positions and damage reads all new positions.

## Engine replication fixture

Run `unreal/Scripts/test-native-proof.ps1` only after building the module and
generating `/Game/CommandProofGenerated/Maps/CommandProof`. The separate
`VisualProof` map remains the art review scene. The runner starts one
headless UE server and two UE game clients, each limited to two cores. Its
default 60-second deadline includes startup. Logs, temporary files, cache and
machine-readable `result.json` stay under ignored `generated/` on the designated non-system drive.

The runner probes UDP port availability on loopback and explicitly binds the
server to `127.0.0.1`, waits for readiness, and requires:

1. Two distinct human owner IDs with accepted move/attack/produce acknowledgements
   and a rejected command against another owner's unit.
2. Both clients and host explicitly reporting movement, damage and production.
3. Host acknowledgement of accepted bot commands through the same model.
4. Matching digests at at least three common replicated revisions across the host
   and both clients; duplicate revision contents must also agree within each log.
5. Successful completion markers and clean exit codes from all three processes.

Cleanup targets only exact processes created by the runner and checks their start
times and executable paths. It does not stop other UE sessions, change firewall
rules, or install anything. Reserve this three-process window with other project
work before running. A bind race after the availability probe remains possible
and must fail the run rather than silently choose a new port.

Snapshots contain the fixture's entire visible state; this digest comparison is
not a cryptographic security claim or proof of all hidden command state. The
fixture has no real navigation, production UI, matchmaking, authentication,
host migration, or adverse-network coverage. Future scale tests must combine
active movement, combat, bots and replication under measured load; budgets remain
undetermined. Art remains unapproved blockout work.
