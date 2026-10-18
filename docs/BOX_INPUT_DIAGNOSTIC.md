# Diagnostic input acceptance

This separate diagnostic branch starts at production `2fdc6cee`. The original
production build and results remain in its original worktree. This instrumentation
must not be merged with gameplay source.

The fixture is a standalone presentation world with an explicitly synthetic,
over-cap population. It verifies viewport-injected key edges reaching the real
bindings and production selection code. It uses the existing `ControlsCursor`
route for the pointer. It does not exercise the real `GetMousePosition` route,
physical hardware, OS cursor movement, focus acquisition, capture, replication,
legal unit production, bots or economy. The final context-order case checks local
dispatch only; production Victory/native regressions supply separate gameplay
evidence. No host receipt is claimed for the diagnostic world.

Actual viewport `HasFocus` and `IsForegroundWindow` are required throughout.
No focus/capture override, cursor movement, binding replacement or automatic retry
is permitted. Unavailable initial focus expires within the overall scenario
deadline; a later unexpected loss stops the fixture. A physical focus transition
and a real lost-release occurrence remain unverified unless independently
observed. Native state tests separately cover lost-release discard/rearm logic.

The 31 fixed cases cover plain click, release-only four-pixel threshold, sticky
return, four directions, press-latched Shift/cap/primary, empty boxes, HUD start
and end, outside pointer, actual resize/restoration, owner/epoch/phase changes,
duplicate engine edges, all nine groups, visible/paired T and context Move
dispatch. Duplicate edges may be normalized by Unreal; injected totals and actual
callback counts are separate. Expected selection arrays and revision changes are
fixed independently of the production planner/atomic API. Shared baseline setup
is explicitly logged before each case and never increments native counters.

Two captures are requested during confirmed held drags. The HUD records the
actual production bounds/draw frame for those capture requests. Result review
must bind the exact Source tree, DLL, capture request/completion and PNG hashes.

Run only after exact source and runner review, fresh unchanged admission, and an
instrumented build. Preserve two CPUs (2/6, mask 68), Normal priority, one build
action/NoUBA, 30 FPS, 220-second scenario and 240-second external total bounds,
2 GiB cache/1 GiB temp guards and exact owned-process cleanup. No timeout extension
or automatic rerun on failure. Diagnostic UHT/compiler/engine execution remains
on hold for cloud review and capacity. Production `2fdc6cee` and its passed
83 native tests/rendered Victory are preserved independently.

The pure `BoxInputEvidence.ps1` gate validates all case/phase stimuli, every
observation's context, pointer, held/Shift/armed/dragging state, processed-frame
freshness and native counters. Explicit `before-release` records prove restored
viewport/context and cancellation while still held. Visible then Battlefield T
scope and the final Move type are emitted and checked independently. Epoch
`7 -> 8 -> 7` is diagnostic snapshot setup; it does not restore the controller's
monotonic private epoch or prove a real lifecycle transition.

PNG acceptance uses installed System.Drawing to decode and rasterize the whole
1280x720 PNG and hashes its decoded pixels; header/IEND markers alone cannot pass.
The PowerShell-only `test-box-input-evidence.ps1` preflight supplies clearly
synthetic complete transcripts, stimulus/counter/phase faults and valid/invalid
PNG fixtures. Its passing results are parser evidence, never engine acceptance.
