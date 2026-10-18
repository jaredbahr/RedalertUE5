# Skirmish status readability — issue #18

## Scope

Build on the merged legacy-label and bounded skirmish marker work. Current skirmish row two combines credits, delivered ore and selected-description text, while FitToWidth removes trailing characters until the line fits. Primary ID/HP already has a separate fallback row, but role, cargo, group count and factory rally can compete for remaining width. This is source-observed information loss risk, not a new measured screenshot failure.

Prepare one presentation-only change: separate economy from selection, offer explicit full/compact variants with the same critical values, measure each with the real Canvas font/scale, and choose the first variant that fits. Never silently shorten an ID, health, credit amount or cargo value into another value. Keep any remaining generic clipping clearly separate from successful critical-field presentation. Preserve the panel footprint and shared camera exclusion, marker placement/leaders/brackets, command/input behavior and all resource/simulation rules.

Primary player fields: current credits and delivered ore; selection count; primary ID, kind, HP and cargo; factory rally state when applicable. Control hints must still expose select/order, build costs, Stop, groups and Attack Move without claiming RA2 keybind parity. Diagnostic input/ack logs remain available, with no privacy data added to the UI or repository. No visual art/style expansion is included.

## Existing work reconciliation

PR43 integrated bounded markers, camera/selection/rally/Attack Move. PR48 added legacy label placement; PR49 changed ore continuation. The older dense-layout source at a33038c already contributed leader/rematch work through subsequent integration; replacing the current HUD with that old file would lose newer behavior. Older draft27 is historical group/control work, not an additional HUD implementation to merge wholesale. Reuse current main and the existing panel/layout helpers, retaining issue18's open dense-marker and960-viewport limits.

## Portable dot-cloud task

Proposed production header: unreal/Source/RedalertProof/Presentation/ProofHUDTextChoice.h. It must have no Unreal, platform, filesystem or rendering dependency. Given measured candidate widths in preference order and the available width, return the first fitting index, or no choice. Use a closed width boundary; reject invalid/nonfinite/negative available width and candidate widths; skip malformed candidates rather than let NaN appear to fit. Define empty input, null pointer/count and zero-width behavior explicitly. Do not add tolerance that makes overwide text fit.

Proposed portable tests: tests/hud_text_choice_tests.cpp, using the actual production header. Cover preference order, exact fit, next-representable over/under values, zero and empty inputs, negative/NaN/infinity inputs, and deterministic results. Cloud owns this header/tests only after accepting this assignment; no local overlapping implementation is started. It must not choose labels, fonts, geometry or other style decisions. Deliver a small reviewed patch and hash based on current main; no UE build or visual claim.

Local lane owns ProofHUD.cpp wiring, candidate content and applicable engine/native fixtures. Wait for portable helper delivery before wiring its API; keep command/simulation and panel footprint unchanged.

## Pass criteria and execution gates

1. Independent source review confirms critical full/compact variants retain identical values and current marker/panel/input invariants.
2. Portable production-header cases pass on the cloud worker; report exact counts and no engine acceptance claim.
3. After explicit confirmation that Imagination's smoke allocation released, obtain a fresh bounded host allocation/capacity check before any UE build/native/runtime process. Do not infer release from idle-looking host samples.
4. Build the final frozen clean source. Run focused HUD native cases and targeted1280x720/960x540 skirmish captures with existing original deadlines/assertions, plus representative controls/Stop compatibility only where wiring changes justify it. Verify actual Canvas measurements and independently inspect native-size PNGs: critical fields contained and complete, no panel/world overlap, primary information stays inspectable in the tested dense case.
5. Retain all failures; no automatic retries or deadline widening. Do not close issue18 based only on a helper test or one viewport. Physical input, army-scale, final art and arbitrary-edge acceptance remain separate.

## Human trial

Preserve the existing known-working trial checkout, DLL and14 assets as rollback. A refresh requires final merged-source/config/project and asset/cache-path checks plus applicable build/runtime acceptance. The validator-source change also means old asset evidence manifests cannot be promoted as current. Keep new trial preparation separate; do not overwrite rollback while awaiting acceptance. No paid Actions, binaries, raw private logs or new installations are needed.

## Preparation state

This lane contains a plan only. No helper, HUD implementation, engine compile, runtime process or human-trial refresh has run. Host allocation release is pending. Dot cloud ownership must be confirmed by the coordinating task before duplicate work begins.