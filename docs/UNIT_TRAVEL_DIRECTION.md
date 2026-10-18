# Authoritative last-travel direction

Source baseline: `d965aa2163cccc2646ed4b954fabb228a54a1e50` (merged unit-view
grounding). The initial PR63 source was `78bca82d6d76cece9453eb7ac6485e15ca9b7557`.
This correction preserves the initial native failure described below. The change
adds presentation/evidence state and immediate Rhino view alignment. Native
engine, runtime replication and rendered acceptance remain pending for corrected
source. Portable tests are not substitutes for those gates.

## State contract

`FProofUnitRecord::LastTravelDirection` is a reflected `FVector`, carried by the
existing replicated snapshot. Exact zero means no prior actual XY travel in
this unit lifetime. New records, refinery grants, production and resets start
unset. There is no invented spawn-facing angle.

After each positive simulation `Advance`, one bounded pass compares each unit's
final position with its pre-movement position, before damage. Record count and
UnitId correspondence are asserted and tested. Finite scalar X/Y subtraction
results in the host's active floating-point environment define displacement.
A nonzero result sets a normalized XY direction with exact Z=0. Normalization
rescales by direct division through the largest component. The new numerical
helpers have scoped precise-FP protection for MSVC builds, preventing fast-math
reciprocal rewriting while leaving surrounding gameplay compilation unchanged.
Invalid, effective-zero and vertical-only displacement preserve the prior
direction. Existing world-coordinate validation is unchanged.

Distinct endpoint bit patterns can still produce effective-zero displacement
under FTZ/DAZ. This is reachable: the existing tiny-endpoint Move branch copies
the destination, so `(1,0)` to `(1,denorm_min)` can change stored Y bits while
subtraction flushes to zero. Gradual underflow records +Y; effective-zero
subtraction retains the previous heading. The regression executes actual
Submit/Advance and checks both copied endpoint bits and this rule. No epsilon
or bitwise endpoint inequality replaces the scalar displacement criterion.
Clients receive the host's authoritative direction rather than repeating this
arithmetic; cross-platform bit-identical denormal computation is not promised.

Stop, arrival, stationary fire, harvesting and deposit waits retain direction.
Movement completed before lethal damage records its final direction; already
dead records do not acquire one. Owner removal preserves surviving records.
Reset/rematch creates fresh defaults even if IDs are reused. Updates continue
when Revision has saturated at MAX_int32.

Two actual simulation steps can travel out and back before a snapshot is
observed. That stationary snapshot still contains the final reverse direction.
Net-zero motion within a single `Advance` retains the prior direction by
contract. Client position history, camera motion and order destinations do not
determine this field. No movement, economy, damage, range, timing or command
path reads it.

The diagnostic digest appends `d:id,x,y,z;` at existing `%.17g` precision for
nondefault direction, after the optional rally tag. All three components are
committed, including unexpected nonzero Z. Exact-zero defaults preserve prior
digests. MD5 remains a diagnostic equality check, not authentication. Update all
participating builds together; mixed old/new snapshot schemas are not accepted
as a compatible network session.

## Current Rhino display contract

Only live Combat records whose actual component mesh is the loaded Rhino proxy
are aligned. A global loaded flag alone is insufficient: structures or miners
may fall back to that proxy, and cube meshes have no approved visual front.
Those other views retain their existing orientation.

The current imported Rhino's accepted visual-front contract is mesh-local +Y:

- Current uasset SHA256: `07a04de36a31f5cfcdc2d730ce72283b903592946fc5868e330739356ed86fb4`
- Recorded import MD5: `d3098209fd883b6d9f431acf9e2c7cd6`
- Retained FBX SHA256: `4cab57cf75cb9ad7cb7a8bc83147a13025fe52cd5558df6b783d7400af27bb76`
- The generator barrel points along Blender -Y. Retained FBX model rotation is
  approximately 180 degrees about Z with scale 100; transformed bounds match
  the current UE local bounds. In the hash-linked original runtime image,
  identity component rotation and the recorded camera make local +Y screen
  right, where the visible muzzle points.

This is a scoped visual/import-provenance contract, not a formal semantic vertex
label or permission to assume the same axis after an asset replacement/reimport.
The original captured image was inspected unchanged. No asset is modified here.

The adapter uses the component's actual scaled/rotated +Y axis, including signed
nonuniform scale and tilt, then applies an axis-neutral world-Z quaternion on
the left. It preserves tilt and scale. Unset, nonfinite, nonplanar direction or
degenerate forward projection leaves orientation unchanged. A newly created
stationary view with valid stored state aligns immediately, without prior
client observations. Repeated frames do not intentionally accumulate rotation.
The final transform feeds the already accepted grounding calculation; model
X/Y and ground Z remain authoritative.

This is immediate last-travel display alignment. The current static mesh turns
as a whole. Simulated turn duration, an independently aiming turret, RA2 spawn
facing, physical-input acceptance and full RA2 unit fidelity remain separate.

## Portable checks

From the repository root, with a C++17 compiler and installed OpenSSL 3
development headers/libcrypto for the model/digest tests:

```sh
bash tests/run_last_travel_direction_tests.sh all
bash tests/run_unit_view_facing_tests.sh all
bash tests/run_snapshot_digest_bridge_tests.sh
bash tests/run_bot_match_replay_tests.sh
bash tests/run_bot_match_replay_tests.sh --sanitized
```

The focused native-model test body also runs against the production model via
the portable UE subset: 279 checks in each of five FP modes for normal,
optimized and ASan+UBSan builds.
It covers actual movement/rounding, record identity, lifecycle, economy waits,
numeric extremes, saturation and real-MD5 direction goldens. Eight deliberate
semantic/numerical faults are rejected, including destination-derived direction
on a real representable-step case, reciprocal rewriting and a numeric no-op. The pure production-facing helper passes 17,553
mathematical checks in unchanged-default, gradual and FTZ modes, and 17,549 in
DAZ and combined modes, for each build configuration. Its explicit reciprocal
fault control fails the intended subnormal cardinal oracle. The complete-match serializer includes the new
field, preserving its all-visible-state replay promise.

Separate source-bound review executes the exact extracted Pawn loop with test
doubles: 309,142 checks per mode, with the unchanged baseline and 16 meaningful
fault controls rejected in all three modes. Independent transformed-front and
eight-corner oracles cover actual mesh eligibility, tilted/signed-scale input,
stationary/new views and camera/world-transform changes. This is explicitly
portable emulation, not native UE transform execution. Pure world yaw preserves
ground Z mathematically, so grounding-before-yaw is not misrepresented as a
detectable semantic fault; final-transform source ordering is checked instead.

In the cloud executor, LeakSanitizer could not run under ptrace. Sanitized runs
explicitly used `ASAN_OPTIONS=detect_leaks=0`, retaining ASan and UBSan. Leak
checking is unverified. No runner installs software or changes provider policy.
See [the real-MD5 bridge](../tests/README.snapshot_digest_bridge.md) for ASCII
encoding limits, RFC vectors and failure behavior.

## Preserved native blocker and numerical correction

The initial source built successfully, then passed 40 of 41 Skirmish tests.
`LastTravelDirection` failed three assertions. Case-labelled evidence recorded
`denorm_min` input bits `1` and MXCSR `0x1fbf` with FTZ=0 and DAZ=0. MSVC
14.44.35229 used `/fp:fast`; exact built-model-helper disassembly showed a shared
`1.0 / Scale` followed by two multiplies despite direct division in source.
The reciprocal overflowed and finite guards retained the previous direction.
Case 3, negative subnormal X, was also wrong but the old loose sign oracle
missed it. No Controls, rendered or networking acceptance was performed in that
failed run. The original diagnosis, hashes, raw rows and disassembly are retained
separately from corrected-source evidence.

The correction scopes `float_control(precise,on,push)` / `pop` to only the new
model recorder and small facing helper, guarded by `_MSC_VER`. It preserves
finite/zero semantics and division order without changing module flags or
runtime MXCSR. The facing helper has the same vulnerable source pattern; the
retained native disassembly proves the model recorder only. Test-only precise
regions keep numerical assertions meaningful. The scope follows Microsoft's
[float_control documentation](https://learn.microsoft.com/en-us/cpp/preprocessor/float-control?view=msvc-170).

Strict case-labelled analytic expectations now distinguish every intended
cardinal/diagonal result from the prior heading. Cardinals and zero components
are bit-exact; irrational analytic components permit only two ULPs with mandatory
nonzero values and matching signs. This is an output-accuracy allowance, not a
movement threshold. Computed-zero cases must retain exact prior bits. Volatile scalar operand loads and result stores prevent the
test's subtraction from becoming an endpoint-equality shortcut. Minimum-normal
and 1e-200 controls must remain nonzero even though naive squaring underflows.
The portable process separately runs gradual, FTZ, DAZ and combined controls,
restoring its original MXCSR; native tests never change the engine's FP mode.
A matching three-failure portable FTZ reproduction was an earlier hypothesis,
not the actual native cause. Both boundaries remain covered.

The corrected native DLL must be inspected to establish that direct divisions
and finite checks survive its actual compiler settings. Portable success does
not prove the MSVC pragma's generated code, nor the final native test result.

## Required native and runtime acceptance

- Build with the intended UE5.7 toolchain/UHT. Run exact new native names
  `RedalertProof.Skirmish.LastTravelDirection` and
  `RedalertProof.Controls.UnitViewFacing`, plus all unchanged expected names.
  Manifests become 41 Skirmish and 16 Controls; only the added names change.
- Also run applicable Commands, Stop and AttackMove regressions because
  `Advance` is shared. Retain existing admission, CPU/storage and runtime bounds.
- The native facing test uses actual UE quaternion composition and transformed
  box bounds; it must verify cardinal sign/order, tilt, signed scale, repeated
  stationary alignment and ground contact. Portable doubles do not prove UE
  conventions. Native MD5 must match the literal canonical goldens.
- A bounded same-asset rendered fixture must show two ordinary movement legs,
  a real direction change, arrival and a stationary interval, with actual
  stored direction, transformed local +Y, full transform/bounds and original
  game-only captures. Retain the earlier readiness failure: the initial
  diagnostic reached bot terminal state before capture and proved no turn.
- A bounded host/client lifecycle fixture must directly inspect a surviving
  stationary headed unit on a newly observing client, with matching actual
  vector and digest across roles. Observe fresh zero state after rematch/epoch
  changes. Rejoining a deferred seat does not itself reseed removed units.
- The existing client snapshot logger is Revision-only gated. A fixture using
  that path cannot claim transport observation at saturated Revision; the
  focused model test covers saturation separately. Wire size, delivery cadence,
  Internet play and large-army costs have not been measured for this field.

Native/runtime observer additions and their private admission wrappers remain
separate from this production patch. No native, PowerShell, network or rendered
pass is asserted by the cloud source package.
