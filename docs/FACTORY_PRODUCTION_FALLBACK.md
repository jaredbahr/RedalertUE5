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

## Remaining acceptance

These are portable handler/model checks, not native Unreal, UHT, replication,
rendered input or physical keyboard evidence. The complete Pawn translation
unit and its real engine types have not been compiled for this patch. The
remaining integration gate is the normal approved UE build plus the relevant
native model/control suites on the integrated source.

In a later authorized skirmish acceptance run, build two owned factories with
distinct rally points. With no factory selected, press `P` and verify the first
factory's legal spawn and rally. Select the second and repeat after earning
enough credits; it must use the second. Check a non-factory selection and a
lost first factory, then confirm the existing insufficient-credit and
no-factory messages. Physical `P` input/focus acceptance remains separate from
scripted handler coverage. Existing launcher admission and cleanup limits
still apply; this change does not authorize a game session.

## Opt-in engine routing fixture (prepared)

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

This fixture is prepared source, not an executed acceptance claim. It still
requires source review, a fresh admitted UE/UHT build, native checks and an
actual three-role run within the existing deadlines before integration.