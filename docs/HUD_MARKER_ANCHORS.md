# Projected marker text anchors

Prepared on `fix/hud-marker-anchors` from main `0c7b07c21b6c794b83973f3735e8eb75dea49d9c`. This is a narrow presentation correction; compilation and rendered verification are pending.

Installed UE 5.7's `Engine/Source/Runtime/Engine/Private/HUD.cpp`, `AHUD::DrawText` (line 933), multiplies `ScreenX` and `ScreenY` by `Scale` when `bScalePosition` is true, then scales the font separately. Our unit/resource markers already use projected viewport pixels. Scaling those coordinates again displaces their labels toward the top-left relative to the brackets, ore crosses and HP bars. At 1280-wide UI scale, a unit-label anchor `(800, 400)` becomes `(704, 352)` with label scale 0.88.

The source diff changes exactly three `bScalePosition` arguments from true to false in `ProofHUD.cpp`: the two ore text lines and the skirmish unit label. Font sizes, projection, marker/HP-bar drawing and gameplay rules are unchanged. No collision layout or art change is included.

Before acceptance, build the exact committed source when the shared heavy-test slot is available, then inspect fresh captures at 1280x720 and a common smaller viewport such as 960x540. Check text anchors against their unit/resource markers, alignment around the panel boundary, and ownership/HP readability. Record the actual viewport sizes and tested revision; earlier captures do not verify this patch. Reuse existing ignored assets without generation or binary uploads.

Only source inspection and `git diff --check` are complete. No engine, graphics or runtime checks have run on this branch. [Issue #18](https://github.com/jaredbahr/RedalertUE5/issues/18) remains open: dense label overlap, edge clipping and broader viewport/human readability acceptance are not resolved by this correction. Existing generic proxies remain unapproved art and scale.
