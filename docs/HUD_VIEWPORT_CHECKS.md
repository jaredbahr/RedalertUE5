# Pending HUD viewport and dense-overlap checks

Prepared fixture support only; no new engine build or render has run on this revision. Historical `8dc1f9f` build/1280 evidence remains in [HUD_MARKER_ANCHORS.md](HUD_MARKER_ANCHORS.md). The three anchor corrections remain exactly three `bScalePosition=false` changes; the new pawn/runner changes only allow an explicit second fixture viewport.

The runner accepts `-Viewport 1280x720` (unchanged default) or `-Viewport 960x540`. It passes that contract to the pawn, waits for the actual configured viewport, and checks both PNG dimensions. Any other preset is rejected; direct unsupported fixture arguments fail the fixture. This does not declare a minimum supported product resolution.

## One pending bounded batch

Imagination owns the heavy-test slot during preparation. Run nothing until the slot is handed back. On a clean reviewed candidate with existing ignored assets, perform one build with MaxParallelActions=1, then exactly one existing Victory run per preset, serialized:

    .\unreal\Scripts\build-native-proof.ps1
    .\unreal\Scripts\test-small-match.ps1 -Scenario Victory -Viewport 1280x720 -TimeoutSeconds 240 -CleanPresentation
    .\unreal\Scripts\test-small-match.ps1 -Scenario Victory -Viewport 960x540 -TimeoutSeconds 240 -CleanPresentation

No automatic retries, larger deadline, new assets or changed resource policy. Abort subsequent runs on failed build or incomplete owned-process cleanup. Retain any run failure as a failure. Normal Victory/bot/economy assertions remain mandatory; rendering a PNG alone does not pass the fixture. Record exact candidate HEAD, clean state, build result, actual viewport, capture freshness/hashes, terminal result and cleanup before visual review.

## Inspect four captures, not just the runner result

Inspect `01-base.png` and `02-result.png` from each viewport at native size. The existing authoritative-command scenario naturally places multiple structures, harvesters and produced units close together in the base and battle. Use those clustered regions to assess crowding; record the visible cluster and selected unit rather than claiming a synthetic army count. If a capture lacks close neighbors, mark the dense check insufficient and prepare a distinct explicit workload before another allocated run; do not call it a dense pass.

Record each check separately:

| Check | Review requirement |
|---|---|
| Unit anchors | Label position follows its unit/brackets/HP bar; reducing font size must not pull the screen anchor toward the origin. |
| Ore anchors | Name and remaining ore stay adjacent to the ore cross at both presets. |
| Panel and viewport edges | Objective, controls, costs, credits, selected status and result/rematch text stay inside the actual image; note marker text hidden by or clamped against the panel and any right/bottom clipping. |
| Dense ownership/HP | Inspect neighboring friendly/enemy markers, selected marker and HP bars. Record every ambiguity/overlap region; a command/PNG runner pass is not a readability pass. |
| Comparison | Compare relationships to markers, not exact battle positions or label IDs from another run. Record the existing CleanPresentation option explicitly. |

The previous 1280 images still show overlapping labels in dense clusters. This preparation adds no collision avoidance, label priority or decluttering. If overlap persists, the outcome is **anchor correction verified, dense readability still unresolved**. Keep #18 open; do not relabel an observed overlap as passed. A narrow anchor PR may be assessed independently, with this limitation explicit, after its smaller-viewport regression check passes. Full issue acceptance still requires readable ownership/HP and bounded layout at the tested viewport.

Human input/focus, final art/scale, Internet behavior and performance remain outside these captures. No new performance or army-size claim follows from the two presets.

Preparation checks completed: independent source review; PowerShell syntax; parameter-only probes for default 1280x720, 960x540 and uppercase 960X540 normalized to the same dimensions; unsupported 800x600 rejected. The existing 240-second default was preserved. These checks launch no engine and do not compile or validate the current C++ fixture changes.
