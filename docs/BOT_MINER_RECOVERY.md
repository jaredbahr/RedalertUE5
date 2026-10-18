# Bot recovery after complete miner loss

The bot could lose all of its harvesters while its refinery survived. The planner
only built a refinery when no live refinery existed, even though the shared
`BuildRefinery` command already grants a harvester. It could therefore spend its
remaining credits on combat units and never resume its economy.

## Narrow behavior change

When the bot has no live owned harvester and at least one valid positive ore
record remains, its existing refinery construction path may build an additional
refinery. A damaged but living owned miner suppresses this recovery. Dead and
foreign miners do not. Ordinary startup without a refinery is unchanged.

The existing requirements still apply: live HQ, refinery cost, two free live
record slots, legal structure placement and a clear miner spawn. The planner
reserves the cost and both records before considering other construction or
production. It does not grant units or credits itself; every intent goes through
the same authoritative command validation used by humans. The next snapshot's
living replacement suppresses further recovery builds, and its Gather order uses
the existing miner rotation. A later complete loss can recover again.

This does not add miner production, selling, refunds, new prices or unit roles.
It cannot recover when funds, two record slots, HQ, ore or placement are missing.
Existing fallback combat/production choices remain unchanged when recovery is
impossible. The decision uses the current bounded, obstacle-free prototype's
ore and placement rules. It is not an assertion of original-game AI parity.

## Tests

Two native groups are added under the existing skirmish prefix:

- `RedalertProof.Skirmish.BotMinerRecoveryPlanner`: loss and survivor predicates,
  foreign/dead miners, stable order, absent/depleted/invalid ore, funds, HQ,
  two-slot capacity, blocked spawn, unchanged snapshots, repeat suppression and
  subsequent loss.
- `RedalertProof.Skirmish.BotMinerRecoveryModel`: accepted ordinary commands build
  both bases; combat kills both bot miners while its refinery survives. With the
  exact recovery cost left, the bot rebuilds through the shared validator. The
  new miner receives Gather, travels, extracts and deposits without more refinery
  construction. No private state injection or forced resources are used.

The portable runner also executes all four existing native bot test bodies:
planner, full model path, refinery planning and multi-miner rotation.

From the repository root, with an existing GCC C++17 toolchain:

```sh
bash tests/run_bot_miner_recovery_tests.sh
```

The runner compiles the actual production `ProofCommandModel.cpp`,
`ProofSkirmishBot.cpp` and `ProofSkirmishRules.cpp` plus the actual native test
bodies. It uses explicit test checks that remain active under `NDEBUG`, strict
warnings and normal / optimized builds. It makes no network calls, installs
nothing and writes temporary executables outside the repository. No hosted
workflow is added or run.

### Portable evidence limits

`tests/portable_unreal/` is a test-only implementation of the small CoreMinimal
and automation subset used here. It is never included in the Unreal module.
It approximates containers, vectors and test registration; it does not execute
Unreal's actual containers, allocation, reflection, serialization or test runner.
The unused snapshot digest routine is discarded at link time; the MD5 shim is
intentionally undefined, so using it accidentally makes linking fail rather than
pretending to verify hashing. There are no changes to production model or digest
code to make this harness work.

At the prepared source, GCC 14.2 C++17 normal and `-O2 -DNDEBUG` runs each pass
287 explicit checks in six test bodies. The same new tests against the unchanged
baseline bot reproduce 11 failures, including missing miner recovery and deposit.
These portable results are source-level regression evidence, not an Unreal build,
runtime, network, performance or original-game fidelity pass. Native build, the
broader skirmish suite and representative bot-match runtime checks remain pending
under the separately coordinated local admission and test budgets.
