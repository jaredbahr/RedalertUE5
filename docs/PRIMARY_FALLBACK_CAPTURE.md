# Controlled second-pass capture

This diagnostic builds on source `8e9a0b2746f3be22efac316857e5a66ebedd67fd`.
The prior 14 screenshots used the first anchor. They did not visually exercise
the below-glyph fallback, even though the native captured-coordinate tests did.

## Setup difference

Run the existing clean 1280x720 Victory readability fixture with the additional
`-PrimaryLabelFallbackCapture` switch. Its fifth capture retains the existing
camera projection target, frozen authoritative snapshot, live selected Combat
unit, full measured ID/HP group and production primary-label placement path.
It is **not an identical replay** of the old C#32 match; selected IDs and other
terminal positions may differ.

Only the explicitly requested fifth screenshot adds three opaque, visibly
captioned UI rectangles forming a U around the selected bracket. The rectangles
are reserved before glyph classification and are drawn with those same bounds.
They leave a two-pixel gap from the selected footprint and a three-pixel gap to
the below preference. They do not exempt any source, glyph, label or leader from
the layout's normal collision checks. UI-covered ordinary glyphs are culled by
the normal occlusion path. This controlled UI setup is not ordinary readability
or physical-input acceptance.

The case is restricted to a 1280x720 viewport and the measured 32x25 primary
group. Unexpected dimensions or an unavailable selected Combat cannot silently
pass: the runner requires the fixture marker and actual `placed=true`,
`passesAttempted=2` primary row. The unchanged match budget remains 240 seconds.
The U geometry also has assertions within the existing 15-test native manifest.

## Required evidence

After an explicit heavy-slot grant: fresh exact-source build, all 15 native
tests, then the existing readability/camera capture runner with
`-PrimaryLabelFallbackCapture -ReadabilityCaptures -CameraEdgeCaptures
-CleanPresentation -Scenario Victory -Viewport 1280x720` and the configured
`-EngineRoot` placeholder.

Retain the head and clean source, DLL/map hashes, launch flags, original PNGs,
fixture snapshot digest, source/occluder bounds and primary size, chosen below
preference, pass count, full label/HP rectangle and leader endpoints. Retain the
existing camera rig/projection observations and frozen-scene checks. Inspect the
fifth PNG for a visible obstruction, intact selected ID/HP group below it and a
clear source-linked leader; JSON success alone is insufficient. Missing pass 2
is failure, not permission to alter an image or loosen a check.

**This diagnostic source, expanded assertions and rendered case are unrun.**
No app-control approval or physical input is required for the offscreen case.
Pending interactive approval and input/focus/terminal/rematch gates are unchanged.
