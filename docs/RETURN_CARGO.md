# Direct miner unloading

The original Red Alert 2 manual describes selecting a miner and clicking a
refinery to force an early return. Reference: [original manual, Game Basics,
printed page 17](https://www.manualshelf.com/manual/games-pc/command-conquer-red-alert-2/user-guide-english/page-24.html),
a third-party-hosted transcription of the publisher's manual. It supports the
qualitative control. The prototype's existing prices, speed, cargo capacity and
instant conversion on arrival are retained; no new balance values are inferred.

## Controls and scope

Select a loaded owned Harvester, then right-click an owned live Refinery. An
all-Harvester group sends the same action for each loaded member; empty members
are skipped. A mixed Combat/Harvester selection retains its existing context
behavior. An empty single Harvester receives a status explanation instead of an
unload request. Ore clicks still issue Gather, and ground clicks still issue Move.

This adds the explicit `ReturnCargo = 9` command. Existing enum numbers and the
command payload shape remain unchanged. The new target is a refinery UnitId;
it is not a resource ID. Host and clients must use a compatible build.

## Authoritative semantics

- Only a loaded, live owned Harvester in a playing skirmish can return cargo.
  The clicked target must be a live owned Refinery. Allied/foreign refineries,
  empty cargo, other roles, dead/missing records and malformed envelopes are
  rejected through the ordinary validator.
- Submission changes the order. The miner travels toward that particular
  refinery, even if another refinery is closer. There is no teleport or immediate
  credit grant. Arrival uses the existing mining travel/deposit tolerance.
- Conversion occurs after movement and combat, with the existing credit and
  gathered-counter saturation rules. A full credit balance leaves the undelivered
  amount in the miner until room becomes available.
- A displaced active Gather order retains its resource ID. Repeated ReturnCargo
  commands retain this same saved ID, including when the refinery target changes.
  After complete deposit, the Gather resumes. A depleted existing source can use
  the existing ore-reselection policy; a removed source stops normally.
- A miner previously following Move, Stop, or no order unloads and then waits.
  This bounded slice does not invent a new mining assignment in that case.
- If the chosen refinery is destroyed before conversion, the forced return is
  canceled and undelivered cargo remains. It does not silently choose another
  refinery. A later explicit ReturnCargo or Gather can recover.
- Move, Stop and Gather replace forced return through existing command semantics.
  Unit death and terminal matches keep their existing behavior.

## Client integration

`ProofReturnCargoSelection` uses the current snapshot and the selection's existing
owner/epoch check. It builds ordinary zero-sequence intent templates and changes
no authoritative state. Owned-refinery context takes precedence over nearby ore
for an all-Harvester selection. Hostile and mixed-selection handling is retained.

The existing group queue coalesces ReturnCargo by unit, normalizes its unused
movement payload, revalidates cargo/refinery before dispatch and preserves its
current bounded cadence. Direct ReturnCargo cancels older queued work for that
unit in the same way as other direct orders. The pawn stamps epoch/sequence and
uses the existing server RPC and acknowledgement route. Request diagnostics name
ReturnCargo explicitly.

The T-selection implementation, ore-reselection function, bot planner and group
dispatch clock are not changed by this slice. The new model command and context
helper are tested alongside the existing selection, ore and miner-recovery work.

## Portable validation

With an existing GCC C++17 toolchain, run from repository root:

```sh
bash tests/run_return_cargo_tests.sh
```

The runner compiles actual production model, rules, bot, selection, queue and
ReturnCargo helper sources, plus native test bodies, in normal and optimized
NDEBUG modes with strict warnings. The explicit assertion counters remain active
under NDEBUG. It uses a test-only minimal UE subset under `tests/portable_unreal/`
and leaves temporary executables outside the repository. No packages, network
access or hosted workflows are required.

The subset is not a substitute for Unreal containers, allocation, reflection,
serialization or the native automation runner. The unused digest routine is
removed at link time; MD5 is intentionally undefined in this harness. The Pawn,
viewport projection, input events and RPC transport are not compiled or executed
by the portable tests.

## Verified portable results and integration

The original-base and merged-main variants each pass 18 native test bodies with
622 active checks in both normal and optimized NDEBUG modes. Independent review
also ran 12 unchanged bot/queue/economy bodies with 443 active checks per mode.
That supplemental harness uses an actual installed OpenSSL MD5 adapter, verified
against five known vectors; it is evidence-only and adds no production dependency.
The dependency-free candidate runner still leaves MD5 undefined.

The exact original-base candidate also passes address/undefined sanitizers with
622 checks. Leak detection is disabled because LeakSanitizer is incompatible
with this executor's tracing; no leak-detection success is claimed.

The integrated source includes merged selection, ore-reselection, bot-miner
recovery and factory-production fallback. Core portable inputs remain identical
to original cloud candidate `837e9a4`: its reported 622 ReturnCargo and 287
miner-recovery assertions per normal/optimized mode remain source-bound portable
evidence, rather than a new Windows compiler run.

## Native and rendered integration

Engine checkpoint `55cba8265910a89e148adb539a94772208a9f17a`, reconciled with
main `4cacdd8`, passed the installed UE/UHT build and all 74 unique native groups:
40 Skirmish, 14 Controls, 5 Gather, 11 Stop and 4 FactoryRally, with zero failures.
The scripted standalone ReturnCargo fixture and ordinary Victory both passed.
Actual PowerShell policy checks passed all original 86 cases plus three
explicit negative-string controls. The existing factory parser's 30 checks
also passed.

The actual fixture proved four forced returns/deposits, three saved-Gather
reloads, an empty group member being skipped and a stopped miner waiting empty
until explicit Gather. It used ordinary commands, actual mining, host model
validation and receipts. See [the acceptance record](RETURN_CARGO_ACCEPTANCE.md)
for exact source identity, timings, economy evidence and retained failures.

This rendered evidence is narrower than the native model tests. It covers
standalone local authority and shared handlers, without physical mouse/keyboard
or remote transport verification. Destroyed-refinery, repeated/retargeted-return,
terminal/reset and saturation cases have native coverage; their wider rendered,
physical/focus and network acceptance remains separate. Prototype PNGs do not
establish final art or full RA2/Yuri economy, AI, timing or faction parity.