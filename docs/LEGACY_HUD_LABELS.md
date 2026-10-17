# Legacy ID and health label placement

The non-skirmish command proof drew each `#ID HP` label directly beside its unit, independently clamping and truncating text at the viewport. Labels could overlap each other and brackets; the default-controls capture shows the #3/#5 cluster. Skirmishes already use bounded placement.

This follow-up changes only legacy presentation. It reserves the existing panel and every fully visible bracket footprint, including stroke width, before placing labels. It sorts primary, other selected, then stable unit ID; measures the full label with the same font/scale used for drawing; and uses the existing 66-candidate linked placement search. A dark background supports contrast. Accepted labels avoid the panel, brackets, earlier labels and leaders. Failure omits the whole world label rather than shortening its identity. Partly clipped or panel-occluded glyphs are hidden rather than clamped into a border pile.

Primary ID and HP also appear in row eight of the existing legacy panel. Panel geometry and camera exclusion remain unchanged. Actual font height and clipping still need engine screenshots. This does not guarantee every dense world label can fit.

## Review and queued tests

- Runner PowerShell syntax passes. The exact ten unique native test names match its expected set with no duplicates or differences.
- Two new native groups call the actual production wrapper. The cluster case recreates overlapping old #3/#5 rectangles at 1280x720 and 960x540 and requires both full labels plus the primary label to fit, with panel/glyph separation, determinism, insets and full measured dimensions. Failure cases cover invalid dimensions/scale, oversized text, cleared stale output and clipped source glyphs.
- These geometric tests use supplied measured rectangles; they do not prove Canvas font measurements or rendered readability.
- Independent source/API review found no blockers. The existing skirmish marker block, shared placement implementation and panel geometry are unchanged.
- No Unreal compilation, native test execution or engine capture has run for this follow-up. Earlier screenshots identify the defect; they do not validate this new source.

## Acceptance queue

Wait behind Imagination and PR #45. Keep PR #45 frozen at `9b9403f51417fcce3de159b779943c49bae31023`. Reconcile this separate branch after PR #45 is accepted, preserving failures and source bindings.

Build the resulting reviewed source and run the exact ten HUD layout tests with zero failures. Verify default controls and Stop screenshots at the actual 1280x720 viewport: the original #3/#5 labels must both be readable, accepted text must not overlap, selected ID/HP must remain readable in the world or panel, and ownership/selection brackets must remain clear. Add targeted 960x540/edge captures through an appropriate fixture before claiming small-viewport acceptance. A representative skirmish capture must retain its existing behavior. Inspect native PNGs with fresh source/DLL/hash/timestamp bindings and independent review.

This patch does not resolve the separate dense-skirmish primary-label omission or close issue #18. No command, input, simulation, art, installation or paid-service changes are included.
