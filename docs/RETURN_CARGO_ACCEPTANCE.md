# ReturnCargo native and rendered acceptance

The published integration is reconciled with main
`4cacdd803220fb1ca1b6bc7a9cbc87ea686f04e8`, including bot-miner recovery and
factory-production fallback. Engine acceptance below was executed at
`55cba8265910a89e148adb539a94772208a9f17a` (Source
`96a2c14b81deba085f6f4bdaaa9033cc5e55b987`). Use that integrated source;
do not mix the older standalone variant's manifests into it. This opt-in
fixture adds read-only observations and shared-handler orchestration, without
new units, resources, timing allowances or authoritative state setters.

## Admission and native pre-gate

Use only the separately approved native execution environment and its existing
installed toolchain. This package does not authorize a launch. Preserve the
existing two-CPU, Normal-priority and owned-process identity/cleanup policies,
900-second build admission cap, 90-second native-test cap, 220-second in-game
scenario deadline and 240-second outer scenario cap. Do not change a limit to
make a failure pass. The existing build script itself is not a 900-second timer;
the approved execution wrapper must retain that build admission limit.

Before rendered acceptance, build the exact integrated source, including UHT,
and run the exact-name/count native manifests for that source path:

- Primary current-main plus compatible ReturnCargo: 40 Skirmish and 14 Controls.
- Original 6d662 plus standalone ReturnCargo alternate: 38 Skirmish and 14 Controls.

Both retain all 12 `RedalertProof.Skirmish.ReturnCargo.*` tests and the two
`RedalertProof.Controls.ReturnCargoSelection` /
`RedalertProof.Controls.ReturnCargoQueue` tests. The primary path additionally
retains the two merged bot-miner-recovery tests. This acceptance fixture changes
no native manifests. The complete existing groups remain required; do not drop
test names or replace their exact-name gates with an aggregate success count.

From the approved checkout, with the engine root supplied by the operator:

```powershell
& './unreal/Scripts/build-native-proof.ps1' -EngineRoot $env:UE_ENGINE_ROOT
& './unreal/Scripts/test-skirmish-model.ps1' -EngineRoot $env:UE_ENGINE_ROOT -TimeoutSeconds 90
& './unreal/Scripts/test-group-controls.ps1' -EngineRoot $env:UE_ENGINE_ROOT -TimeoutSeconds 90
```

Run the pure policy regression script with an already installed PowerShell 7
before spending an engine admission:

```powershell
& './unreal/Scripts/test-return-cargo-evidence-policy.ps1'
```

It invokes the actual PowerShell policy on explicitly synthetic events and a
synthetic validated-context test seam. It does not execute the prerequisite
semantic/digest parser, generate screenshots or substitute for any engine/native
acceptance. PowerShell is not installed in the cloud preparation environment;
these cases were authored and statically reviewed there, not executed. No
translated implementation is presented as a PowerShell test result.

## Offline Windows preflight

The first actual PowerShell run at original source `837e9a4` rejected its first
valid synthetic case: `invalid command identity target`. The test generator's
untyped negative positional arguments (`-1`, `-500`) became strings in
PowerShell argument mode, so its JSON contradicted the strict numeric schema.
Typing the generator's integer fields and position argument fixes that test
representation without changing the production evidence policy or gameplay.
The same 86 authored cases then passed in actual PowerShell. Three explicit
negative-string target, saved-resource and coordinate cases bring the suite to
89 checks while retaining string rejection. Original failure
and corrected output are retained privately; these remain synthetic policy
checks, not the prerequisite semantic parser, native UE or rendered acceptance.

## Opt-in rendered invocation

After the native pre-gate passes, use the existing generated CommandProof map:

```powershell
& './unreal/Scripts/test-small-match.ps1' -EngineRoot $env:UE_ENGINE_ROOT -Scenario Victory -ReturnCargoEvidence -TimeoutSeconds 220
```

`-ReturnCargoEvidence` requires plain Victory and is mutually exclusive with
GroupGather, factory-rally, cooperative-context, readability and camera-edge
fixture modes. It automatically enables the existing refinery/miner semantic
validator. Defaults and other scenarios are unchanged.

This is the existing single rendered standalone authority process. The pawn
projects a cursor and invokes shared selection/context/Stop handlers, then uses
the ordinary Request, sequence allocator, local ServerSubmitCommand route,
authoritative model validator and receipt path. It does not invoke native mouse
or keyboard callbacks. Host and rendered source records are observations within
that local process, not proof of remote RPC serialization or network transport.
The nested result and all phase/capture markers explicitly report:

- `sharedHandlers=1`
- `nativeCallbackInvoked=0`
- `physicalInputVerified=0`
- `networkTransportVerified=0`

## What must be demonstrated

The fixture uses only the starter miner A and the miner B granted by the ordinary
paid refinery build. It runs 19 ordered phases and exactly eight miner commands:

1. Gather A, then direct ReturnCargo A after a real load. Observe loaded travel,
   one exact authoritative deposit and a subsequent reload without a new Gather.
2. Select A plus still-empty B, then issue the ordinary group refinery action.
   The queue must contain exactly one unstamped command for A. Observe its second
   travel, deposit and saved-Gather reload. B must receive no ReturnCargo.
3. Queue Stop A and verify it stays loaded. Gather B. Once B loads, select both
   and queue two ReturnCargo commands through the existing group dispatch cadence.
4. Observe both authoritative deposits. A, whose prior order was Stop, must wait
   empty and stationary. B must reload from its saved Gather source without a new
   command. Explicitly Gather A to restore normal play, then capture the selection.
5. Complete the unchanged ordinary Victory criteria and result screenshot.

Normal combat and production may run during travel/reload observations. Exact
fixture sequences are bound to the corresponding phase's `nextBefore`; unrelated
commands can occupy gaps between batches. No extra miner command is allowed.
The single and loaded-only batches each accept one ReturnCargo; the group batch
accepts two. There are exactly four forced travel/deposit proofs and three
saved-Gather reload proofs.

## Evidence boundary

`Get-RefineryMinerEvidence` runs first without modified digest, semantic-state,
refinery-grant or receipt gates. Its validated common epoch/revision maps bind
every ReturnCargo phase to both host and rendered source state. ReturnCargo's
owner, epoch, miner/refinery/resource IDs, cargo and player totals must agree.
Each authoritative command must match exactly one ordinary Request and receipt.

The host observer reads snapshots immediately around real Submit/Advance calls.
ReturnCargo Submit must not move the miner, change cargo or grant money. A later
loaded movement step must make positive progress toward the selected refinery.
Only an immediate authoritative deposit row can prove conversion location:
squared distance to the selected refinery must be at most 1, the unchanged
existing 1 cm arrival tolerance. A later rendered empty-cargo frame cannot
substitute for that boundary, and no travel or timing tolerance is widened.

Deposit contributors bind unit IDs and return sequences. Their cargo loss must
exactly equal both the credit delta and gathered-counter delta, while total
ore+cargo+gathered is conserved. Duplicate credit observations are rejected;
simultaneous per-miner rows sharing one Advance boundary count that credit once.
A chronological economy ledger starts at the validated selected checkpoint,
connects every subsequent owner-1 command and unique deposit boundary, and must
match every common published credit/gathered total. Equal-delta offsets cannot
substitute for source-bound absolute balances; ordinary production spending
between publications remains allowed.
`resumeResource` records accepted-command provenance, not a read of private model
orders. Saved-Gather behavior is proved by a later authoritative reload at that
resource, with actual ore consumption and no intervening command for that miner.
Reported ore totals must fit the nearest common published checkpoints and agree
with an exact authoritative revision when present. The first resumed phase must
show the reported load without an intervening deposit cycle.

The launcher still requires the fresh `01-base.png` and `02-result.png` captures,
and adds `03-return-cargo.png`. Existing PNG signature, minimum size, requested
viewport dimensions, per-run freshness and SHA-256 recording are retained. Review
the actual image for the two selected miners and the existing HUD/input scope.
A log marker alone is not screenshot validation.

## Executed acceptance and remaining scope

All eight admitted heavy stages passed at engine checkpoint `55cba826`, with
DLL SHA-256 `FD405AD7FCCF49336ECF175A3E6E80339D1A75719356FDD77AB831FB7635E70B`.
A later documentation-only publication must retain identical Unreal and test
trees; it is not a separate engine execution.

| Check | Result |
| --- | --- |
| Installed UE editor/UHT incremental build | Passed, 17.428 seconds |
| Native Skirmish / Controls | 40 / 14 exact groups passed |
| Native Gather / Stop / FactoryRally | 5 / 11 / 4 exact groups passed |
| ReturnCargo standalone Victory | Passed, 112.973 total / 112.888 scenario seconds |
| Ordinary standalone Victory | Passed, 66.514 total / 66.346 scenario seconds |
| Actual PowerShell policy | 89 checks passed: original 86 plus three negative-string controls |
| Existing factory-production parser | 30 synthetic checks passed |

All 74 native groups are unique, with zero failures. The ReturnCargo run
validated all 19 phases and eight miner commands. Miner 2 returned at sequences
6 and 11, then miners 2 and 11 returned at sequences 16 and 17 to refinery 10.
Their deposits occurred at revisions 106, 145 and shared revision 198; the
shared 100-credit boundary counted once. Three subsequent resource-1 reloads
proved saved-Gather behavior, while the stopped miner waited empty until an
explicit Gather. Absolute balances and total ore/cargo/gathered conservation
passed the actual raw PowerShell policy. The group PNG shows two selected
miners with primary 11 carrying 50 after its reload. All five fresh original
1280x720 PNGs across both runs were reviewed. Ordinary Victory supplies baseline
completion/receipt evidence; it did not enable the semantic ReturnCargo logger.

Every stage used three fresh total/selected-CPU samples at or below 80% and no
competing engine/build work. The serial build used one action/NoUBA; owned
processes used CPUs 2 and 6 at Normal priority. Existing 900/90/220/240-second
build/native/scenario/outer bounds remained unchanged. Polling repairs are not
continuous confinement. Available individual cleanup records and a fresh final empty heavy inventory
were verified. Skirmish/Controls legacy results omit individual cleanup fields.
The allocation was released. Generated evidence,
cache and binary outputs remain ignored on D:; preserved human trials were not
refreshed.

Original failures remain private: the first synthetic policy rejected string
negative arguments; the first UHT-successful build then hit MSVC C4458 because
an observer parameter named `Owner` shadowed `AActor::Owner`. Typed test helpers
and the parameter-only rename to `TrustedOwnerId` fixed these without weakening
numeric validation, warnings or gameplay rules. Fresh acceptance used the
corrected source. Core model/helper/native-test and portable inputs remain
identical to original cloud candidate `837e9a4`; the earlier 622/287 portable
checks per mode are not a new Windows portable-compiler execution.

This is standalone local-authority shared-handler proof. The inherited generic
top-level transport label does not establish a network test; actual topology
and nested `networkTransportVerified=false` define the scope. Physical input,
focus, remote RPC serialization, internet, performance, cross-viewport/final-art
acceptance and wider RA2/Yuri fidelity remain separate. The 12 native ReturnCargo
model groups cover repeated/retargeted returns, overrides, lost refineries,
partial/saturated deposits, source depletion/removal, combat ordering,
validation atomicity, sequence/rate behavior and terminal/reset state; those
wider cases were not all rendered.