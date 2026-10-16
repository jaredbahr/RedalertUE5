# Focused injected control-group check

Prepared only. No build or runtime job is authorized until the shared heavy-test slot is allocated. This test branch adds no gameplay feature and changes neither main nor issue status.

## Dependencies and purpose

`test/nine-group-input` starts from PR #30 publication `41f3020fe285ebbdcb3d8dac051b3272a36b8f29`. Commit `97aea64` carries the existing opt-in PR #29 fixture from the conflict-free combined tree `82637e84257e4d2b2f3fc7e49c3483d554ce656e`; it does not merge either sibling branch or claim the combination is compiled. The existing nine-group source passed the separate PR #30 model/handler-compatibility batch at `9bc6a4a`; those results do not validate this combined test source.

The focused mode reuses `UGameViewportClient::InputKey`, timestamped press/release edges, the normal pawn bindings and callback counters. Synthetic pointer coordinates remain explicitly projected fixture input. It must not call group assignment/recall handlers directly or mutate authoritative units/resources to obtain a pass.

It depends on normal graphical startup and PR #29's processed-input bootstrap readiness gate. That gate builds but its final source has not passed runtime validation. The original stage-zero timeout, separate stage-16 move-receipt failure and later pre-fixture startup timeouts remain retained in the original worktree and [PR #29 evidence](https://github.com/jaredbahr/RedalertUE5/blob/feat/input-binding-fixture/docs/INPUT_BINDINGS.md). This focused mode bypasses the unrelated production/gather/combat/rematch stages; it does not diagnose or erase their failures.

## Bounded scenario

1. Wait for the existing ready predicates, including the current registered input component, completed normal input-stack frame, all six bound digit delegates, fixed viewport, replicated owned HQ/Harvester and zero pending assets. Do not delay, replay or weaken the gate to obtain a pass.
2. Native injected LMB selects the two stationary starting units in alternating order. Left Ctrl plus each digit 4–9 saves its expected singleton. Require one group callback per chord, then Ctrl and digit release.
3. Before every plain-digit recall, inject an empty-ground click at a prechecked point and require empty selection. Recall all six groups and compare exact IDs, callback increments and released-key state.
4. Overwrite each group 4–9 in turn with the opposite starting unit using Right Ctrl plus digit. After every overwrite, recall all six groups. Only the intended slot's expected ID changes. This detects aliases/misrouted slots even though the fixture uses only two existing units.
5. Require 12 assignments and 42 recalls: exactly 54 group callbacks, 54 LMB callbacks and 240 press/release edges. All other command/zoom/rematch callbacks must remain zero. Capture initial and final recalled state; verify fresh 1280x720 PNGs and hashes. Require at least ten identical shared epoch-1 digests across server, rendered client and passive observer.

The explicit server fixture flag suppresses bot decisions; ordinary host validation/simulation remain active. No build, production, harvest, movement, combat or rematch is requested by this mode. The full legacy fixture remains a separate default with its original assertions and epoch-2 requirements.

## Allocated-slot command and acceptance limits

After explicit slot allocation, record the exact clean source commit, build once using the existing D: toolchain policy, and reuse the fourteen validated ignored assets only after path/SHA256 verification. Then run once:

```powershell
./unreal/Scripts/test-input-bindings.ps1 -HigherGroups -CleanPresentation -InputTrace
```

The focused scenario is bounded to 90 seconds, client wall time to 85 seconds, existing per-stage readiness bound to 60 seconds and warmup to 300 seconds. Use normal priority, existing CPU/cache/temp/storage guards, offscreen rendering and identity-checked cleanup. One configured cache condition; no retained/fresh retry matrix. Preserve any timeout with its exact phase. If startup blocks the check, stop and release the slot without relaunching or inferring a root cause.

Every result must state `inputSource=engineInjection`, `pointerSource=projectedSynthetic` and physical-input/OS-focus/positive-real-focus-pan verification false. Pan and simulated focus-loss assertions are not exercised; the existing finish/cleanup path still flushes input through LostFocus. Even a passing focused run cannot establish human keyboard/mouse behavior, Alt-Tab, actual OS focus or resolve PR #29's failures. Issues #17/#18 remain open; the intended result closes only the narrow injected digit-4–9 dispatch gap.

## Preparation checks completed

Independent source review found and corrected a focused-only readiness-evidence gap: force the actual ready sample before the READY marker so a prior one-second sample cannot falsely retain a blocked predicate. This is not a diagnosis or repair of the original unexplained timeout. No remaining source blocker was found; compilation is still pending.

`./unreal/Scripts/test-input-binding-policy.ps1` passed 18 synthetic acceptance cases, including missing slot/round evidence, wrong callback/edge counts, held keys, incomplete binding masks and false physical-input claims. `./unreal/Scripts/test-input-startup-parser.ps1` passed 15 parser cases. These scripts extract only pure functions through the PowerShell AST; neither executes runner setup, launches UE or dispatches input. Syntax and `git diff --check` also passed. No C++ build, rendered run, new-digit dispatch or physical-input result is claimed.
