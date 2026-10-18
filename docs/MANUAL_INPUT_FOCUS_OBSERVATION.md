# Current focus in native-input diagnostic rows

Issue #17 source fix. Status: source review, engine build and existing regression
tests passed. Real native-callback focus ordering and human input acceptance
remain unverified.

`RecordManualInput` previously printed focus/foreground flags cached by the
Pawn's preceding tick. A viewport change before the next tick could therefore
give an input row stale flags. T already refreshed before type selection; most
other callbacks did not. The recorder now calls the existing
`ObserveViewportFocus` before formatting the diagnostic row. This samples the
viewport at record time, after the callback's action, not the historical OS-event
timestamp. It does not assert that the input originated from a person.

The existing observer also clears a pending T gesture when focus or foreground
is absent, including a missing viewport. Such cancellation can now occur at the
input record rather than waiting for the next Pawn tick. The same policy already
applies on every tick and before T selection; no new input gating is introduced.
Commands, selection actions, costs, runtime budgets and log formats are unchanged.
Detailed input rows remain capped at 24, detailed focus transitions at eight,
and both counters still saturate safely. Callbacks without a manual-input row,
such as Escape and T release, are outside this change.

## Reproduce the source tests

From the repository root, with an existing Python 3 and C++17 compiler:

```text
python unreal/Scripts/test-manual-focus-observation.py
python unreal/Scripts/test-manual-focus-observation.py --mode optimized
```

`CXX` selects a GCC/Clang-compatible compiler. Optional `--mode sanitized` uses
AddressSanitizer and UndefinedBehaviorSanitizer; it requires those compiler
capabilities. The test installs nothing and does not launch Unreal. It extracts
the two complete production methods and supplies explicit minimal viewport,
string, logging and gesture adapters. It checks every previous/current focus
combination for all recorder key labels, missing/restored viewport, the existing
T pre-refresh, log caps, counter saturation and bounded outcome text. Checks
remain active with `NDEBUG`. No new Unreal automation name is registered, so the
native exact-name manifests do not change.

## Integrated regression evidence

Tested checkpoint: `b0034a5c5e54324bae9f84aa13915a5b83f194f8`, reconciled with
main `8a145884f9e8382788d4ac9b1bd61222661c2083`. Source tree:
`40f8b4e7417508a6324524db7427aa2d31aaa5fd`; built module SHA-256:
`2A8E3718D060534C85C0F969353C5BD70B002D88F3D99E1A76C3D2A86F767F53`.

- Existing installed-engine build passed. Native MapTypeSelection (3),
  TypeSelection (3) and Controls (14) passed all 20 exact named groups.
- Map and visible shared-T controls scenarios passed with a dedicated host,
  rendered client and passive observer on native UDP loopback. Their raw policies
  were independently reparsed, and eight original 1280x720 captures were reviewed.
  Total/scenario/warmup times were 53.549/42.210/11.110 seconds and
  49.845/40.235/9.382 seconds, within the unchanged 390/90/300 limits.
- The initial map-controls attempt failed its strict 90-second scenario limit
  (91.799 seconds observed). Its rendered frame stopped advancing after the first
  capture request; cache messages continued, but the exact blocking operation is
  unproven. No manual recorder callback ran before that failure. The failed result
  and raw logs remain retained; a later pass does not relabel it.
- Load-blocked admissions launched no engine. Accepted stages used fresh admission,
  serial allocation and the existing bounded process policy. Final inventory was
  empty. Polled affinity/priority repairs do not establish continuous confinement.

The 1,274-check source tests in normal, optimized and sanitizer modes were reported
by the cloud reviewer, not rerun on this Windows host. Their extracted production
methods and test file match the integrated checkpoint. The UE regression fixtures
exercise existing selection/gesture policies and shared handlers with synthetic
focus; they do not invoke the changed native recorder callbacks. They establish
compilation and regression coverage, not real OS-event ordering, physical input,
internet behavior, performance, or final-art acceptance. Human and rollback trial
checkouts were preserved without rebuild or launch.

## Remaining native-callback and human acceptance

Build the exact integrated source with the normal admission controls, run the
existing native/controls tests, and record its source/module identity. During a
separately approved ordinary human session, Alt-Tab away and return, perform a
real action, and correlate its native counter, input row and accepted receipt
where applicable. Capture the actual focus/foreground state and any activation
click requirement. Keep the existing log caps and session bounds. A source test
or engine-injected callback cannot prove physical input. Preserve
`physicalInputVerified=false` until independent human observations are recorded;
do not convert the launcher's automation receipt into a physical-input claim.
