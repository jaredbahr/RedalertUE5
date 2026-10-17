# Projected marker text anchors

Prepared on `fix/hud-marker-anchors` from main `0c7b07c21b6c794b83973f3735e8eb75dea49d9c`. This is a narrow presentation correction. One build and the 1280x720 Victory render have passed; smaller-viewport verification remains pending.

Installed UE 5.7's `Engine/Source/Runtime/Engine/Private/HUD.cpp`, `AHUD::DrawText` (line 933), multiplies `ScreenX` and `ScreenY` by `Scale` when `bScalePosition` is true, then scales the font separately. Our unit/resource markers already use projected viewport pixels. Scaling those coordinates again displaces their labels toward the top-left relative to the brackets, ore crosses and HP bars. At 1280-wide UI scale, a unit-label anchor `(800, 400)` becomes `(704, 352)` with label scale 0.88.

The source diff changes exactly three `bScalePosition` arguments from true to false in `ProofHUD.cpp`: the two ore text lines and the skirmish unit label. Font sizes, projection, marker/HP-bar drawing and gameplay rules are unchanged. No collision layout or art change is included.

The initial verification plan required building the exact committed source and inspecting fresh captures at 1280x720 and a common smaller viewport such as 960x540. The completed portion is recorded below; the smaller viewport is pending. Check text anchors against their unit/resource markers, alignment around the panel boundary, and ownership/HP readability. Record the actual viewport sizes and tested revision; earlier captures do not verify this patch. Reuse existing ignored assets without generation or binary uploads.

Source inspection and `git diff --check` passed before the explicitly allocated build/render batch below. [Issue #18](https://github.com/jaredbahr/RedalertUE5/issues/18) remains open: dense label overlap, edge clipping and broader viewport/human readability acceptance are not resolved by this correction. Existing generic proxies remain unapproved art and scale.

## Completed bounded validation

Clean tested source: `8dc1f9ffca54fb64bf75eefc525b66bdd7c23b5a`, using the existing UE 5.7.4 installation and ignored assets in the integration checkout. One build succeeded: four actions, one parallel action, 10.12 seconds total / 8.93 executor. No asset generation or installation occurred.

One `test-small-match.ps1 -Scenario Victory -TimeoutSeconds 240 -CleanPresentation` run, `small-match-victory-20261017-010406-277`, passed in 68.156 scenario / 68.451 total seconds. Actual winner was team 0; all six required human/bot build/gather/production/attack flags passed. The actual viewport and both fresh PNGs were 1280x720, offscreen, optimized shaders, 30-FPS target. The target is configuration, not a measured performance result. Cleanup errors were empty; owned UE processes exited.

Both `01-base.png` and `02-result.png` were visually inspected. Unit labels are anchored above their markers and ore labels beneath their crosses, correcting the previous scaled-coordinate displacement. The result capture shows the Victory banner and rematch hint. Dense groups still overlap labels. `-CleanPresentation` is an existing fixture option that suppresses engine screen messages while retaining logs; its use is explicit and is not a source change in this fix.

The fixture currently requires 1280x720 in `ProofRTSPawn.cpp`; this batch did not change its acceptance code to run a smaller viewport. That part of the original verification plan remains pending. This narrow 1280 anchor result does not close #18 or claim physical-input, final-art, Internet or performance acceptance.

[Curated build/render evidence and capture hashes](evidence/hud-marker-anchors-20261017.json). Captures remain locally in `generated/small-match-victory-20261017-010406-277/screenshots` under the integration checkout; no generated binaries or raw images were uploaded. This publication commit adds documentation/evidence after the actual tested source revision. No PR or main merge is implied.
