# Focused injected control-group check

The single allocated build/runtime attempt passed at clean `bff79aede5d54ca9e850f134e17629954e046252`. No retry occurred. All owned processes exited and the shared heavy-test slot is released. This test branch adds no gameplay feature and changes neither main nor issue status.

## Dependencies and purpose

`test/nine-group-input` starts from PR #30 publication `41f3020fe285ebbdcb3d8dac051b3272a36b8f29`. Commit `97aea64` carries the existing opt-in PR #29 fixture from the conflict-free combined tree `82637e84257e4d2b2f3fc7e49c3483d554ce656e`; it does not merge either sibling branch. The first combined build/run is recorded below. The separate PR #30 model/handler-compatibility batch at `9bc6a4a` remains evidence only for its earlier recorded source.

The focused mode reuses `UGameViewportClient::InputKey`, timestamped press/release edges, the normal pawn bindings and callback counters. Synthetic pointer coordinates remain explicitly projected fixture input. It must not call group assignment/recall handlers directly or mutate authoritative units/resources to obtain a pass.

It depends on normal graphical startup and PR #29's processed-input bootstrap readiness gate, adapted to require all six higher-digit bindings. The focused gate passed in this run; the later full PR #29 scenario remains unverified. The original stage-zero timeout, separate stage-16 move-receipt failure and later pre-fixture startup timeouts remain retained in the original worktree and [PR #29 evidence](https://github.com/jaredbahr/RedalertUE5/blob/feat/input-binding-fixture/docs/INPUT_BINDINGS.md). This focused mode bypasses the unrelated production/gather/combat/rematch stages; it does not diagnose or erase their failures.

## Bounded scenario

1. Wait for the existing ready predicates, including the current registered input component, completed normal input-stack frame, all six bound digit delegates, fixed viewport, replicated owned HQ/Harvester and zero pending assets. Do not delay, replay or weaken the gate to obtain a pass.
2. Native injected LMB selects the two stationary starting units in alternating order. Left Ctrl plus each digit 4–9 saves its expected singleton. Require one group callback per chord, then Ctrl and digit release.
3. Before every plain-digit recall, inject an empty-ground click at a prechecked point and require empty selection. Recall all six groups and compare exact IDs, callback increments and released-key state.
4. Overwrite each group 4–9 in turn with the opposite starting unit using Right Ctrl plus digit. After every overwrite, recall all six groups. Only the intended slot's expected ID changes. This detects aliases/misrouted slots even though the fixture uses only two existing units.
5. Require 12 assignments and 42 recalls: exactly 54 group callbacks, 54 LMB callbacks and 240 press/release edges. All other command/zoom/rematch callbacks must remain zero. Capture initial and final recalled state; verify fresh 1280x720 PNGs and hashes. Require at least ten identical shared epoch-1 digests across server, rendered client and passive observer.

The explicit server fixture flag suppresses bot decisions; ordinary host validation/simulation remain active. No build, production, harvest, movement, combat or rematch is requested by this mode. The full legacy fixture remains a separate default with its original assertions and epoch-2 requirements.

## Allocated-slot command and acceptance limits

The following command ran once after explicit slot allocation, a successful build and SHA256 verification of fourteen reused ignored assets. These are reproduction instructions, not an automatic request to repeat the passing run:

```powershell
./unreal/Scripts/test-input-bindings.ps1 -HigherGroups -CleanPresentation -InputTrace
```

The focused scenario is bounded to 90 seconds, client wall time to 85 seconds, existing per-stage readiness bound to 60 seconds and warmup to 300 seconds. Use normal priority, existing CPU/cache/temp/storage guards, offscreen rendering and identity-checked cleanup. One configured cache condition; no retained/fresh retry matrix. Preserve any timeout with its exact phase. If startup blocks the check, stop and release the slot without relaunching or inferring a root cause.

Every result must state `inputSource=engineInjection`, `pointerSource=projectedSynthetic` and physical-input/OS-focus/positive-real-focus-pan verification false. Pan and simulated focus-loss assertions are not exercised; the existing finish/cleanup path still flushes input through LostFocus. Even a passing focused run cannot establish human keyboard/mouse behavior, Alt-Tab, actual OS focus or resolve PR #29's failures. Issues #17/#18 remain open; the intended result closes only the narrow injected digit-4–9 dispatch gap.

## Preparation checks completed

Independent source review found and corrected a focused-only readiness-evidence gap: force the actual ready sample before the READY marker so a prior one-second sample cannot falsely retain a blocked predicate. This is not a diagnosis or repair of the original unexplained timeout. No remaining source blocker was found before the allocated build.

`./unreal/Scripts/test-input-binding-policy.ps1` passed 18 synthetic acceptance cases, including missing slot/round evidence, wrong callback/edge counts, held keys, incomplete binding masks and false physical-input claims. `./unreal/Scripts/test-input-startup-parser.ps1` passed 15 parser cases. These scripts extract only pure functions through the PowerShell AST; neither executes runner setup, launches UE or dispatches input. Syntax and `git diff --check` also passed. Those pure checks alone establish no C++ build, rendered run, dispatch or physical-input result.

## Single allocated execution result

Installed UE 5.7.4 built clean `bff79ae` in six actions: 51.41 seconds total / 42.28 executor (`generated/build-higher-group-input.log`). The unchanged source then passed `generated/higher-groups-input-20261017-000032-172/result.json`: 89.080 seconds total, 28.739 warmup and 60.210 scenario, within the fixed 300/90-second runner bounds. No failed attempt, automatic retry or assertion/deadline relaxation occurred.

The runner required all 11 aggregate and 54 slot-specific assertions. Exact log inspection confirms each digit 4–9 received nine press/release pairs: one Left-Ctrl initial save, one Right-Ctrl overwrite and seven plain recalls. There were six Left-Ctrl pairs, six Right-Ctrl pairs and 54 LMB pairs: 240 total edges, 54 group callbacks and 54 LMB callbacks. Other callbacks stayed zero, as did requested command sequence and pending gameplay orders. All exact selected-ID checks passed after each overwrite; final Ctrl/digit states were released.

Ready sample 10 at 8.981 seconds recorded failedMask 0, all twelve compilation-manager counts zero, registered current pawn input, processed input frame 272 and all-six-digit binding mask 63. Host, rendered client and passive observer agreed at 154 shared epoch-1 revisions. Host completion reported epoch 1 / duel 0. Cleanup errors were zero. Independent evidence review found no blocker for this scoped result.

Two fresh 1280x720 captures were hash-validated and inspected: `higher-groups-initial.png` selects Harvester #4 after recalling group 9; `higher-groups-overwritten.png` selects HQ #3 after overwriting and recalling that slot. Both display injected/human-unverified labeling and the 1–9 hint. Generic mesh reuse does not establish final unit art/scale or role likeness. Captures/logs/assets/binaries/caches remain ignored locally.

The fourteen asset copies matched their existing source SHA256 hashes; no art was generated. Project/temp/cache paths stayed explicitly on D: using the existing runner policies, normal priority, two-CPU affinity assignment/repair and 30 FPS offscreen configuration. Warmup CoreLimit is 4, worker inheritance is unverified, and polling is not hard confinement. This is correctness evidence, not a performance or zero-C:-writes claim. `generated/resource-handoff.json` confirms no owned UE/build process remained and releases the slot.

This closes the narrow supported engine-injection gap for Ctrl/digit 4–9 save/recall, modifier release and independent overwrite at this exact combined source. It does not validate the unexecuted full #29 scenario, explain its original failures, establish physical input/real focus, approve a merge or close issues #17/#18.
