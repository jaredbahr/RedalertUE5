# Current focus in native-input diagnostic rows

Issue #17 source fix. Status: portable source tests passed; engine build, real
focus-event ordering and human input acceptance are still required.

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

## Native and human acceptance still needed

Build the exact integrated source with the normal admission controls, run the
existing native/controls tests, and record its source/module identity. During a
separately approved ordinary human session, Alt-Tab away and return, perform a
real action, and correlate its native counter, input row and accepted receipt
where applicable. Capture the actual focus/foreground state and any activation
click requirement. Keep the existing log caps and session bounds. A source test
or engine-injected callback cannot prove physical input. Preserve
`physicalInputVerified=false` until independent human observations are recorded;
do not convert the launcher's automation receipt into a physical-input claim.
