# Avoid rebuilding a refinery after its economy is exhausted

Baseline: `952670dacae7c4bc550af6ecd1608bbe73b16098`.
This is a small bot spending-policy correction, not a command-validator fix or
an original-game balance calibration. Native UE and rendered acceptance for
this candidate remain pending.

## Decision

The existing planner rebuilds a missing refinery whenever it has the funds,
HQ, two record slots and a valid site/spawn. That also spends 100 prototype
credits after every ore patch is empty and no surviving own miner carries
cargo. The new refinery and granted miner then have no resource income to
recover.

A missing refinery now requires either:

- A resource with a positive ID and positive remaining ore, using the existing
  miner-recovery definition; or
- Positive cargo on a live, owned Harvester.

A dead or foreign miner, or cargo on another unit kind, does not qualify. A
surviving refinery with all miners lost still triggers the existing replacement
rule only when usable ore remains. Available ore still permits the normal
opening and later recovery. All site, spawn, HQ, funds, capacity, ordering,
sequence, command-budget and timing rules are unchanged. The command model,
prices, resource quantities and unit statistics are untouched.

When a futile refinery is skipped, the existing factory/production/attack
planning continues. This does not reserve credits, add a new spending priority
or prevent a later decision from rebuilding if its snapshot offers income.
Cargo recovery preserves an opportunity to unload; it does not guarantee a
profitable investment. A 50-capacity miner can carry less than the 100-credit
refinery price. The planner cannot see hidden orders, and this patch does not
resume a stopped miner or introduce a new Gather/ReturnCargo order.

## Regression evidence

The two existing native test names are extended, not renamed or added:

- `RedalertProof.Skirmish.BotMinerRecoveryPlanner`
- `RedalertProof.Skirmish.BotMinerRecoveryModel`

The unchanged focused portable runner executes those bodies plus the four
existing bot suites against the actual command model, rules and planner through
the existing test-only UE subset. It passes 375 checks in normal, optimized and
ASan+UBSan configurations. New tests against the unchanged baseline fail 19
named assertions, including the public-command no-income case. Leak detection
is disabled for the cloud executor's ptrace limitation; leak checking is not
claimed.

The prepared-snapshot controls distinguish absent/depleted/invalid-ID ore,
positive ore, empty/loaded cargo, live/dead/foreign/non-miner cargo, stable
record order, repeated decisions, retained production and factory construction.
Existing funds, capacity, spawn and ordinary miner-loss coverage stays intact.

Two additional model traces use only public seeding, accepted Build/Gather/
Produce/Attack/Move commands and `Advance`; they never edit a model snapshot.
Actual harvesting exhausts both ore patches. Actual hostile damage destroys
the last refinery. The empty case keeps ten live own records and 250 credits,
then plans Produce rather than a pointless refinery. The loaded case retains
an existing Gather order through refinery loss; after the accepted replacement,
that same order deposits its cargo. Exact spend and gathered-credit changes,
zero remaining ore/own cargo and a still-playing match are asserted.

These traces inspect a fresh planner's intents after a separately commanded
setup. Intents are submitted with that model owner's next valid sequence;
the fresh planner's sequence is not presented as an autonomous takeover.
They are public-command reachability and policy tests, not autonomous bot-match
or runtime-input evidence. Their positive simulation steps include large
explicit advances, so they are not a wall-clock gameplay timing measurement.

The unchanged six-setup complete-match replay suite also passes in normal and
optimized configurations, 930,588 assertions each. Each setup reaches its
required valid terminal result and repeats with exact visible-state histories.
Reported match durations (58.1–92.3 seconds here) are simulated seconds, not
execution times, reliability estimates or performance measurements.

## Reproduce and integrate

From the repository root with the existing C++17 toolchain:

```sh
bash tests/run_bot_miner_recovery_tests.sh
bash tests/run_bot_match_replay_tests.sh
```

The native Skirmish expected-name manifest remains exactly 41 tests because
both extended test names already exist. Run that unchanged native suite under
the existing admission/resource/deadline policy after source integration; the
portable adapter is not proof of UHT/MSVC/UE execution. Keep native/runtime
acceptance separate from this cloud source result. No selection, Pawn, HUD,
input, assets, network schema or runtime fixture paths change.
