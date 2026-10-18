# HUD readability candidate

Status: scoped build/model/runtime/visual gates passed at clean `2f3e5947b9907db023a9c27079ca43166fccf84f`. See [acceptance and retained failures](HUD_READABILITY_VALIDATION.md). Dense individual HP/tether ambiguity and rematch-hint contrast remain open under issue #18. The full successful batch must not be automatically repeated.

## Bounded behavior

Skirmish world annotations use measured rectangle groups. Placement tries at most 66 candidates within 64 UI-scaled pixels of the preferred position, reserves the status panel, terminal/rematch text and eligible marker footprints, and keeps accepted groups inside the viewport with a gap. Stable ordering prioritizes the primary selection, other selected units, resource groups, then other unit health bars.

Only the primary selection gets a world identifier; other units retain team/selection brackets and compact health bars. The primary unit's numeric HP and cargo also remain in the status panel. Moved groups have short tethers. Groups that cannot fit are omitted. Partly offscreen or HUD-occluded glyphs are suppressed rather than moved into a border pile. The non-skirmish command-proof presentation retains its existing behavior.

This is provisional classic RTS readability consistent with [art direction](ART_DIRECTION.md), not approved RA2 UI or final art. Accepted rectangles do not overlap, but glyphs can overlap one another, tethers can cross, and omitted groups can still make a crowded scene insufficiently readable. Those limitations need explicit visual judgment; no arbitrary-density or army-performance claim is made.

## Capture observations

The Victory-only `-ReadabilityCaptures` runner switch adds three camera-only terminal views to the normal base/result images: left edge, right edge and just below the status panel. The same live own selected anchor must project within two pixels of each required target. The finished snapshot digest must remain unchanged; no units or resources are moved or spawned. Existing gameplay assertions and the 220s internal/240s wall deadlines remain required.

`PROOF_HUD_LAYOUT_CAPTURE` records one observation per requested UI screenshot. The runner requires five records and five fresh PNGs of the actual requested dimensions. Unit totals satisfy live = eligible + culled, culled = offscreen + HUD-occluded, and eligible = placed groups + omitted groups. Resource records satisfy eligible = placed + omitted groups. Eligibility describes geometry only; it does not establish pixel readability. Omission counts remain observations and are allowed by the parser, so parser success alone cannot establish acceptance. PNG write/freshness checks are separate from the layout record.

## Queued validation after slot handoff

Use one clean the designated non-system drive candidate checkout with the existing generated proof assets available locally; do not generate replacements or upload binaries. Build the exact candidate source once before tests. Preserve each command's output, result JSON, cleanup evidence and screenshot hashes. Run sequentially, with the existing two-CPU/normal-priority policies and the designated non-system drive cache/temp paths. Polling limits are not hard confinement and do not imply zero system-drive writes.

```powershell
& .\unreal\Scripts\build-native-proof.ps1
& .\unreal\Scripts\test-hud-layout-model.ps1 -TimeoutSeconds 90
& .\unreal\Scripts\test-small-match.ps1 -Scenario Victory -Viewport 1280x720 -CleanPresentation -ReadabilityCaptures -TimeoutSeconds 240
& .\unreal\Scripts\test-small-match.ps1 -Scenario Victory -Viewport 960x540 -CleanPresentation -ReadabilityCaptures -TimeoutSeconds 240
& .\unreal\Scripts\test-controls-proof.ps1 -CleanPresentation -TimeoutSeconds 90 -WarmupTimeoutSeconds 300
```

Stop on the first failed command or incomplete cleanup. Retain the failed evidence and diagnose before any explicitly justified retry; do not extend deadlines. A fresh build is mandatory because the cached integration module belongs to an earlier source. The five new automation cases exercise the actual placement helper, including dense reservations, edges/displacement, invalid inputs, deterministic history and priority/failure. The legacy controls run covers the preserved non-skirmish path. Authoritative model/runtime code is unchanged, so another lifecycle batch is not part of this presentation-only queue.

## Acceptance mapping

| Gate | Required evidence before integration |
|---|---|
| Source identity | Clean exact candidate commit; reviewed final source; main/head guards; no gameplay, art or generated-file changes |
| Build and layout invariants | Successful native build; exactly five named layout tests pass; verified owned-process exit |
| Two real viewports | Both Victory runs pass all existing assertions, projection checks, five fresh PNG checks and five count records; all owned processes exit |
| Visual judgment | Root and independent reviewer inspect all ten native-size images, compare base/result against retained baseline, and record eligible/placed/omitted counts alongside conclusions |
| Player meaning | Primary identifier and numeric HP/cargo are legible; bars/tethers remain associated with their units; team/selection cues are useful; ore text and terminal/rematch text remain legible; no accepted group paints over reserved UI or another accepted group |
| Edge behavior | The same selected anchor remains at each validated edge/panel target without border ghosts; intended suppression and any omitted primary annotation are documented, with panel HP still visible |
| Legacy controls | Existing rendered command-proof controls regression passes and capture retains prior behavior |

[Issue #18](https://github.com/jaredbahr/RedalertUE5/issues/18) remains open. Decide its exact remaining acceptance only from the actual captures and omission/association assessment; a layout-test pass or cleaner image alone is insufficient. If this bounded change is acceptable but the broader issue still has gaps, integrate the scoped improvement and retain those gaps explicitly.

[Issue #33](https://github.com/jaredbahr/RedalertUE5/issues/33) remains open and unresolved. This candidate neither diagnoses nor repairs startup timing. Physical mouse/keyboard, Alt-Tab, final art/scale, Internet play and active-army performance are outside these gates.
