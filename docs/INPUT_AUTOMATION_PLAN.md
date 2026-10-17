# Engine input automation plan

The supported route below is now implemented and exercised by an opt-in fixture. See [executed input evidence](INPUT_BINDINGS.md) for the exact clean source, two passing runs, inspected captures, retained failures and remaining limits. Engine injection uses a projected synthetic pointer; physical input and actual OS focus transitions remain unverified. No OS input injection, physical cursor movement, window activation or focus/security setting changes were used.

## Supported route

Installed UE5.7 source provides `UGameViewportClient::InputKey(const FInputKeyEventArgs&)`, then controller `InputKey` and `UPlayerInput` processing. A dedicated opt-in fixture can submit key press/release events through that route and let subsequent normal frames dispatch the existing pawn `BindKey` callbacks. Resolve the keyboard's device/local-player mapping with `IPlatformInputDeviceMapper`; record it. The method's return value alone is not evidence that a callback ran.

Use the existing projected `ControlsCursor` seam for synthetic pointer coordinates. Do not call `SetMouseLocation`: installed source routes it through `FSceneViewport::SetMouse` to `FSlateApplication::SetCursorPos`, which moves the actual cursor. Avoid simulated clicks through SceneViewport mouse-button handlers because those also enter focus/capture behavior.

Use a new explicit fixture flag rather than claiming the existing direct-handler controls automation covers binding dispatch. Existing `IsControlsAutomation()` disables optional pan, so that flag cannot prove pan handling. Preserve the bounded dedicated-host/rendered-client/passive-observer harness, real UDP loopback, deadline, 30 FPS target, cache/temp guards and exact owned-process cleanup. Queue engine jobs behind the shared heavy-test slot.

## Required assertions

1. Inject LMB, wait a processing frame and observe the actual native callback counter and exact selection. Release and verify key-up. A projected synthetic pointer must be labeled as such.
2. Hold Shift across a processed frame, click a second owned unit, then toggle it off. Assert selection IDs and modifier cleanup.
3. For groups 1-3, hold Ctrl, press/release the number, release Ctrl, clear selection, then recall unmodified. Assert exact group contents and callback counts; do not invoke the shared group handler directly as evidence of native binding coverage.
4. Inject RMB for a group ground order and enemy attack. Require accepted host receipts, expected eligible-unit movement/damage, matching replicated snapshots and all-unit selection highlights in captures. Check structures remain immobile and generic Harvesters do not attack. Include a group recall after death and an epoch reset with reused IDs.
5. Inject wheel presses, verify the native callback count and camera width, then release each key.
6. With pan opted in, inject and hold an arrow, verify `IsInputKeyDown`, then assert no camera movement whenever the observed viewport focus or foreground predicate is false. Release and verify the arrow clears.
7. Call viewport `LostFocus` only as an explicitly simulated notification and verify pressed-key cleanup under the existing flush setting. This does not establish an actual OS focus change.

Report `inputSource=engineInjection`, `pointerSource=projectedSynthetic`, `physicalInputVerified=false`, and `osWindowFocusVerified=false`. Keep callback counts separate from direct-handler scenarios and record injected edges so synthetic native callback counts cannot be mistaken for human input.

## Limits and remaining desktop acceptance

The current pan gate reads real `FViewport::HasFocus()` and native `IsForegroundWindow()`. An offscreen test can cover suppression when those are false, but cannot establish positive focused pan, actual activation, mouse capture or Alt-Tab behavior. Pure pan tests already cover movement math. Direct viewport key injection bypasses Slate's real focus routing and cannot prove desktop background-click suppression. Do not add an undisclosed synthetic foreground override to turn these into passing desktop claims.

Positive pan and real focus transitions remain a bounded human desktop playtest, unless a separately labeled virtual-focus unit test is added for the predicate itself. Neither would establish physical hardware input automatically.

Source inspected: installed UE5.7 `InputKeyEventArgs.h`, `GameViewportClient.cpp` (`InputKey`, `LostFocus`), `PlayerController.cpp` (`InputKey`, `SetMouseLocation`), `UserInterface/PlayerInput.cpp` (`ProcessInputStack`, modifier matching), and `Slate/SceneViewport.cpp` (`HasFocus`, `IsForegroundWindow`, `SetMouse`). No plugin or new input driver is required.
