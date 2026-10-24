# Command-card validation controller

PR72 preparation only. This controller has not compiled or run the command card in Unreal. It never verifies physical input or clears PR71 acceptance. Its default invocation prints preparation requirements and launches nothing.

`run-stage.ps1` offers fixed `Build`, `NativeHUD`, `Render1280`, `Render960`, `Input1280` and `Input960` cases. Execution requires all of the following:

- A clean isolated source checkout, exact Git/C++ identities, all tracked-source hashes, installed executable hashes and the fourteen existing authored-content hashes.
- A separately recorded independent review approving that exact binding SHA256. `prepare-bindings.py` writes `REVIEW_REQUIRED.json` with `approved=false`; it cannot grant approval.
- Current coordinator confirmation that owned lanes are drained, no competing game or validator, and actual hashed qualification/admission receipts.
- A full 600-second quiet window **before** an unextended 20-minute stage reservation, followed by fresh dispatch admission. Resource-only samplers retain their original frozen identity; their receipts never become PR72 gameplay evidence.

## Binding and receipt format

`prepare-bindings.py --help` lists the required local paths. It only reads committed source, binaries, existing content and the original resource samplers. It writes a private binding for the chosen future runtime root; it neither materializes that root nor invokes Unreal. Keep these path-bearing files outside public Git.

The prerequisite JSON contains hashed file references shaped as `{"path":"absolute local file","sha256":"lowercase SHA256"}` under `readiness`, `readinessOutcome`, `admission`, `admissionOutcome` and `lease`. Native outcomes are the supervisor's **returned** outcome, including receipt-write completion, rather than an earlier raw exit file. The lease records `head`, `ownedLanesDrained=true`, UTC `createdUtc`/`expiresUtc` exactly 1,200 seconds apart and a local `cancelPath`. Creation follows qualification; dispatch admission follows creation and is no older than 60 seconds.

After a genuinely successful build, add `build` (the actual `PAYLOAD_RESULT.json`) and `buildOutcome` (its `RETURNED_OUTCOME.json`) to the next prerequisite bundle. The checker reconciles their common case directory, source head, build interval, fresh DLL/module bytes and module BuildId. Empty, stale, truncated, changed, forced-cleanup and incomplete receipts fail closed.

Pass the reviewed binding, independent review, actual prerequisite bundle and their hashes to `run-stage.ps1 -Execute -OwnedLanesDrained -CoordinatedWindow` in a fresh dedicated PowerShell 7 process. These switches attest current coordination; they are not a resource reservation service. Create a new cancellation file to revoke a running lease. Never reuse a run directory or synthesize a missing success receipt.

## Bounds and ownership

The PR72-only supervisor derives from reviewed source SHA256 `0ffb82e568a1ae7bce90e751621a2f994fa92a6e7cd13442f4d65aaada1092c6`. It leaves that source and PR71's pins untouched. It creates the worker suspended, assigns its owned Windows Job, binds native identity and sets `0x44`/Normal before resume. It captures at most 1 MiB per output stream. Only its retained process/job handles grant termination authority. Failure-only member observations grant query/synchronization access and never select a process for signaling.

Fixed enclosing limits are admission 20, qualification 650, build 900, native 120 and render/input 220 seconds. Qualification must still contain all 600 quiet seconds. Native HUD retains its 90-second inner limit; input retains 180 seconds beginning at launch, including startup. Each case's monotonic origin precedes binding/setup/parsing. Completion at or beyond its deadline fails, including late natural exits. Cleanup has one aggregate five-second budget; a surviving child, forced cleanup, lease cancellation, incomplete output or uncertain final ownership fails acceptance. No grace period follows root exit.

The worker and Python command-line checker release only their own console attachment to avoid leaving an owned console host behind; redirected output remains captured. They do not find or stop console hosts by name. Deliberate surviving-child rejection remains tested.

Synchronous filesystem/OS calls cannot be forcibly cancelled by this primitive. Overruns fail on return, and prelaunch binding/setup has no separate preemption watchdog. Record this limitation during independent review; do not describe the controller as a hard wall-clock guarantee for every API stall.

## Checks and human evidence

`tests/command_card_binding_tests.py` exercises strict receipt parsing, timing/identity, resource chronology, source review, logs/capture metadata and actual build-file reconciliation with inert data. `test-owned-controls.ps1` launches only short fixtures and one deliberately rejected worker; it exercises native containment, cancellation, output limits, timeout, survivor rejection and output completion. Neither test launches Unreal or establishes engine acceptance. Synthetic PNG metadata checks do not prove that an image renders correctly.

Use the physical-input checklist in [the validation plan](../../../docs/COMMAND_CARD_VALIDATION.md) at both viewport presets. A normal zero-exit session, actual ready dimensions, fresh logs and zero owned survivors are necessary. Explicit tester observations remain separate; unreached terminal/rematch cases stay pending.
