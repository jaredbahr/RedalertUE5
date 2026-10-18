# Factory production fallback

## Behavior

`P` uses the selected owned live factory when one is selected. Otherwise it
uses the first owned live factory in the replicated unit array, matching the
existing [small-match control](SMALL_MATCH.md#run-and-controls). "First" means
array order, not nearest factory, lowest ID or primary-factory designation.

Previously each eligible factory overwrote the fallback, so two ordinary paid
factories used the last factory when nothing, an HQ, a Harvester or a missing
unit was selected. Distinct rallies made the wrong choice observable in the
produced unit's spawn and subsequent movement.

The handler now remembers the first eligible factory while continuing to look
for the selected eligible factory. Ownership/liveness filtering, selected
factory precedence, no-factory rejection, receipt counting and the legacy
non-skirmish request remain unchanged. The authoritative model still decides
prices, capacity, legal spawn positions, credit rejection and rally movement.
No primary-factory UI, production queue or new gameplay rule is introduced.

## Source-bound portable regression

Run on Linux from the repository root with Bash, Python 3, GNU coreutils
(`timeout`) and GCC (or a C++17 compiler/linker accepting the same flags):

```sh
bash tests/run_factory_production_tests.sh
```

The runner extracts the complete current `AProofRTSPawn::Produce` definition
unchanged into a temporary include. Narrow test-only controller/world/request
adapters exercise that method, and the actual `ProofCommandModel.cpp` and
`ProofSkirmishRules.cpp` validate the emitted requests. The runner prints
SHA-256 identities for the method and its production source inputs. A changed
method boundary fails explicitly rather than selecting a partial method.

The dedicated `tests/factory_production_shim` never enters the engine module.
Its `FMD5` declaration intentionally has no implementation: section garbage
collection removes the unused digest routine; a test that starts depending on
it will fail to link instead of accepting a fake digest. Rejection checks
compare every currently defined visible snapshot field directly.

Verified against baseline `eda97228e0679363484487b2732c4d061bfd00df`:

- Strict C++17 with `-Wall -Wextra -Wpedantic -Werror`: 508 active checks pass.
- Optimized `-O2 -DNDEBUG`: the same 508 checks pass. Checks do not use `assert`.
- The unchanged baseline challenged by the same tests: 28 failures per mode,
  including wrong factory ID, wrong spawn and wrong post-rally movement.
- Two factories built and rallied using ordinary accepted commands, with
  distinct rallies, cover no selection, HQ, Harvester, stale/missing ID,
  selected first and selected last. Reversed snapshot order verifies that
  fallback follows array order while explicit selection still wins.
- Snapshot-only routing controls cover dead/negative-HP and foreign first or
  last factories, foreign teammate ownership, invalid/unregistered controller
  owners, three factories including selected middle, all-ineligible and empty
  snapshots. These injected snapshots are routing tests, not reachable-match
  evidence.
- Real one/no-factory setups, exact production cost, insufficient-credit
  rejection with no visible mutation or delayed creation, unchanged combat
  receipt counts, missing controller/game-state paths and legacy non-skirmish
  model acceptance are also covered.

To challenge a separate baseline source tree with the same test bodies:

```sh
bash tests/run_factory_production_tests.sh /path/to/baseline-source-tree
```

This argument changes the production source inputs, not the test bodies or
adapters. The paths in source identity output remain repository-relative.

## Native and runtime acceptance

The integrated source was executed at engine checkpoint
`914aec71df59751f0340b9c362258cd6463c09eb`, Source tree
`f212106972634f37ec3ea53f8aca6712e4ba46ca`. A later documentation-only
publication must retain identical Unreal and test trees; it does not represent
a separate engine execution.

| Check | Result |
| --- | --- |
| Installed UE editor/UHT build | Passed, 8.283 seconds |
| Native skirmish / factory rally / controls | 28 / 4 / 12 exact groups passed, zero failures |
| New two-factory three-role Coop | Passed, 117.015 total / 116.939 scenario seconds |
| Existing single-factory three-role Coop | Passed, 119.703 total / 119.519 scenario seconds |
| Offline production parser | 30 checks passed, including 29 invalid-evidence mutations |
| Existing rally parser / process policy | 32 / 8 offline checks passed |

The two-factory run paid for factories 11 and 18. Empty selection produced
unit 20 at factory 11 (sequence 8); selecting factory 18 produced unit 23
there (sequence 10); selecting HQ 3 produced unit 26 at factory 11
(sequence 12). Every production debited exactly 50 credits and used the legal
spawn of the chosen factory. Each case has five later movement checkpoints
shared by the dedicated host, rendered owner-1 client and NullRHI owner-2
observer. All eight fresh 1280x720 PNGs from the two runs were reviewed in
original form. Their scope is functional prototype evidence, not art approval.

Each heavy stage used three fresh total/selected-CPU samples at or below 80%
and no competing engine/build jobs. The serial build used one action and
NoUBA; owned processes used CPUs 2 and 6, Normal priority and bounded cleanup.
Affinity/priority polling repairs are recorded; polling is not continuous
confinement. Build/native/scenario/total bounds remained 900/90/220/240 seconds.
Generated evidence, caches and binaries remain ignored on D:.

The complete `Produce()` method and its portable test/model/rules inputs remain
identical to original cloud checkpoint `384790d8f312734e93b3bf5bbf01f3bf42ee2df5`.
The 508 checks per compiler mode and 28 baseline failures above are that
source-bound portable evidence, not a newly executed Windows compiler run.
The engine run proves actual shared handlers and loopback replication; it
claims no physical `P`/focus input, internet, performance, final-art or
rendered factory-loss/no-factory/insufficient-funds acceptance. Those negative
routing/payment cases remain portable checks; native rally tests also cover
atomic failed production and factory death/reset behavior.

## Opt-in engine routing fixture

`test-small-match.ps1 -Scenario Coop -FactoryProductionEvidence -CleanPresentation`
adds a separate three-role loopback fixture. It leaves the existing
`-FactoryRallyEvidence` fixture and parser unchanged. The new fixture pays for
two factories, sets distinct rallies, and invokes the actual `Produce()` handler
with empty selection, the second factory selected, then an owned HQ selected.
Construction, Gather, rally and Produce requests use the ordinary controller
route. Credits come from normal mining and deposits. There are no authoritative
snapshot setters, injected credits or direct fixture calls to model Submit.

A bounded read-only host recorder checks both 150-credit builds, both rallies,
and all three 50-credit productions against exact snapshot changes and legal
spawn selection. The separate parser binds request selection, receipt sequence
and epoch, exact new unit, ordered eligible factories and later rally movement
to common host/rendered/observer snapshots. Only the observed proof unit is
protected from the ordinary defense planner during its movement window; earned
credit and replication waits still allow defense decisions. Three additional
PNGs identify the cases. These are scripted shared-handler checks; physical `P`
and focus testing remain separate.

`test-factory-production-evidence-policy.ps1` exercises a valid synthetic
three-case context and 29 invalid-evidence mutations without launching UE.
The runtime runner separately validates full raw three-role state semantics;
synthetic parser checks alone never represent engine acceptance.