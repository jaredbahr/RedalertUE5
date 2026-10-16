# Command-contract experiment

`prototype/` is original, dependency-free JavaScript groundwork for a possible host-authoritative command adapter. It exercises one shared validation path for human and bot commands. It is not an Unreal project, UE replication implementation, internet-ready server, or proof of eight players or large armies.

## Run

From the repository root with Node.js (verified here with v24.19.0):

```powershell
node --test prototype/simulation.test.mjs
node prototype/smoke.mjs
```

No package installation is required. Both commands exit after completion. Localhost TCP access must be permitted by the execution environment; the sandbox initially returned `EACCES`, while an approved run passed. Do not change firewall/security settings to make this experiment run.

## Contract and boundaries

- The host owns credits, production, positions, health, and orders. Clients submit newline-delimited JSON commands, never state snapshots.
- Each connection receives a host-assigned identity: first `human`, second `bot`. This is a two-slot loopback test fixture, not authentication or a final player limit. The server binds only `127.0.0.1` on an ephemeral port.
- Commands are `{seq,type:'move',unit,x,y}`, `{seq,type:'attack',unit,target}`, or `{seq,type:'produce'}`. Sequence numbers start at one for each identity and must increase exactly by one. Unknown fields and command types are rejected. A well-shaped, correctly sequenced command consumes its sequence even when gameplay validation rejects it; malformed envelopes and sequence failures do not.
- Both drivers obey ownership, living enemy target, bounded finite position, credit, and command-allowance checks. Production costs 50 toy credits and completes immediately. Units move 0.5 toy distance per manual tick and deal 10 damage within distance 2. These are experiment constants, not proposed game balance.
- `createSimulation({commandsPerTick,unitsPerPlayer})` and `createHost(...)` accept experiment configuration. The default allowance is 32 commands per identity per manual tick, and the smoke uses six initial units per identity. Neither value is a product unit budget. Frame buffering and queued output have defensive bounds; large snapshots can exceed those bounds and disconnect a client.
- Each host tick broadcasts a complete JSON snapshot. The smoke compares both client copies with the host after every tick. It checks equality within this process, not deterministic simulation across machines.

Files: `simulation.mjs` contains state and rules; `loopback.mjs` contains the TCP fixture and ordered client helper; `smoke.mjs` combines movement, combat, bot decisions, and snapshots; `simulation.test.mjs` tests valid and rejected actions plus two-client consistency.

## Executed evidence

An approved local run passed all six tests. A separate smoke run returned:

| Measurement | Observed |
| --- | --- |
| Initial toy units / clients / manual ticks | 12 / 2 / 24 |
| Ticks with movement / damage | 16 / 10 |
| Ticks with movement and damage together, with bot commands and two snapshots | 10 |
| Bot commands accepted | 96 |
| Matching client snapshots | 48 |
| Snapshot JSON payload bytes, including framing | 55,006 |
| Whole smoke wall time | 50.44 ms |

The timer includes localhost setup, command round trips, assertions, and serialization. It does not pace a real-time game loop. Payload bytes exclude TCP/IP overhead, command traffic, and acknowledgments. These small observations are correctness evidence only; they establish no frame-rate, latency, bandwidth, or unit-capacity budget.

## Next validation gates

The toy simulation has no collision, pathfinding, terrain, fog, production queues, reconnect/resynchronization, session discovery, authentication, encryption, persistence, or Unreal adapter. Its client helper expects strict request/response ordering and is intended only for these controlled scripts. Do not expose it to the internet.

Before choosing practical unit budgets, implement and measure the same combined workload in the chosen UE configuration with active movement, combat, bot decisions, and actual network replication. Agree on hardware, frame-time, memory, traffic, and command-latency thresholds; test adverse network conditions and hosting/connectivity separately. Internet play, eventual eight-player games, and large armies remain ambitions with exact per-player/total counts undetermined. This experiment does not resolve those choices.
