# Skirmish status readability — issue #18

## Scope

Build on the merged legacy-label and bounded skirmish marker work. At the baseline, skirmish row two combined credits, delivered ore and selected-description text, while FitToWidth removes trailing characters until the line fits. Primary ID/HP already has a separate fallback row, but role, cargo, group count and factory rally can compete for remaining width. This is source-observed information loss risk, not a new measured screenshot failure.

Prepare one presentation-only change: separate economy from selection, offer explicit full/compact variants with the same critical values, measure each with the real Canvas font/scale, and choose the first variant that fits. Never silently shorten an ID, health, credit amount or cargo value into another value. When no complete critical variant fits, show an explicit unavailable/overflow state and fail critical-field acceptance for that viewport. Never present silently clipped critical values as a successful fit. Keep remaining generic hint clipping separate from critical-field presentation. Preserve the panel footprint and shared camera exclusion, marker placement/leaders/brackets, command/input behavior and all resource/simulation rules.

Primary player fields: current credits and delivered ore; selection count; primary ID, kind, HP and cargo; factory rally state when applicable. Control hints must still expose select/order, build costs, Stop, groups and Attack Move without claiming RA2 keybind parity. Diagnostic input/ack logs remain available, with no privacy data added to the UI or repository. No visual art/style expansion is included.

## Existing work reconciliation

PR43 integrated bounded markers, camera/selection/rally/Attack Move. PR48 added legacy label placement; PR49 changed ore continuation. Current main already contains bounded leader/rematch behavior. This plan does not assert that a33038c is an ancestor; replacing the current HUD with that older file would lose newer behavior. Older draft27 is historical group/control work, not an additional HUD implementation to merge wholesale. Reuse current main and the existing panel/layout helpers, retaining issue18's open dense-marker and960-viewport limits.

## Portable dot-cloud task

Proposed production header: unreal/Source/RedalertProof/Presentation/ProofHUDTextChoice.h. It must have no Unreal, platform, filesystem or rendering dependency. Given measured candidate widths in preference order and the available width, return the first fitting index, or no choice. Use a closed width boundary; reject invalid/nonfinite/negative available width and candidate widths; skip malformed candidates rather than let NaN appear to fit. Define empty input, null pointer/count and zero-width behavior explicitly. Do not add tolerance that makes overwide text fit.

Proposed portable tests: tests/hud_text_choice_tests.cpp, using the actual production header. Cover preference order, exact fit, next-representable over/under values, zero and empty inputs, negative/NaN/infinity inputs, and deterministic results. Cloud owns this header/tests only after accepting this assignment; no local overlapping implementation is started. It must not choose labels, fonts, geometry or other style decisions. Deliver a small reviewed patch and hash based on current main; no UE build or visual claim.

Local lane owns ProofHUD.cpp wiring, candidate content and applicable engine/native fixtures. Wait for portable helper delivery before wiring its API; keep command/simulation and panel footprint unchanged.

## Pass criteria and execution gates

1. Independent source review confirms critical full/compact variants retain identical values and current marker/panel/input invariants.
2. Portable production-header cases pass on the cloud worker; report exact counts and no engine acceptance claim.
3. After explicit confirmation that Imagination's smoke allocation released, obtain a fresh bounded host allocation/capacity check before any UE build/native/runtime process. Do not infer release from idle-looking host samples.
4. Build the final frozen clean source. Run focused HUD native cases and targeted1280x720/960x540 skirmish captures with existing original deadlines/assertions, plus representative controls/Stop compatibility only where wiring changes justify it. Verify actual Canvas measurements and independently inspect native-size PNGs: critical fields contained and complete, no panel/world overlap, primary information stays inspectable in the tested dense case. Targeted cases include large IDs/economy/cargo/group counts, factory rally set/unset, no selection and explicit no-choice behavior; a no-choice critical-field result does not pass that viewport.
5. Retain all failures; no automatic retries or deadline widening. Do not close issue18 based only on a helper test or one viewport. Physical input, army-scale, final art and arbitrary-edge acceptance remain separate.

## Human trial

Preserve the existing known-working trial checkout, DLL and14 assets as rollback. A refresh requires final merged-source/config/project and asset/cache-path checks plus applicable build/runtime acceptance. The validator-source change also means old asset evidence manifests cannot be promoted as current. Keep new trial preparation separate; do not overwrite rollback while awaiting acceptance. No paid Actions, binaries, raw private logs or new installations are needed.

## Historical preparation state

Prepared source now includes the reviewed cloud helper, complete local formatting/Canvas wiring and fail-closed critical-text capture checks. Cloud supplied23 portable checks, passing normal and optimized builds with independent review. Local production-parser44 synthetic checks pass in PowerShell7 and5.1. Three native formatter groups are prepared alongside the original ten HUD cases. No UE compile/runtime or trial refresh has run at this checkpoint. Imagination release at04:28UTC is confirmed; fresh admission is still required.
## Exact cloud assignment contract

Baseline: public main618ac23f5669cd0ef2783c589cc37b2eea0fdcfa, rechecked unchanged before this assignment. Local reviewed plan checkpoint4b90b927e489426f607ba15618895c7606efe953 is not a public baseline. Cloud owns only the following production header and portable test file:

- unreal/Source/RedalertProof/Presentation/ProofHUDTextChoice.h
- tests/hud_text_choice_tests.cpp

Required public API (header-only definitions; C++17):

```cpp
#include <cstddef>
#include <limits>
namespace ProofHUDTextChoice {
inline constexpr std::size_t NoChoice = (std::numeric_limits<std::size_t>::max)();
inline std::size_t ChooseFirstFitting(const double* CandidateWidths,
    std::size_t CandidateCount, double AvailableWidth) noexcept;
}
```

CandidateWidths points to CandidateCount readable doubles in preference order; the caller owns that buffer. Return the first index whose finite nonnegative width is <= finite nonnegative AvailableWidth, otherwise NoChoice. Invalid available width returns NoChoice for the entire call. Null pointer or zero count returns NoChoice without dereference. Skip negative, NaN and either-infinite candidate widths, preserving later valid candidates. Zero and signed negative zero are valid widths/available widths; a zero candidate fits zero available width. Closed exact boundary, no epsilon, rounding, allocation, global state, mutation or dependency on text/font/Unreal/platform/filesystem. Caller supplies valid readable storage for non-null positive count; this function cannot validate pointer extent. No text/label generation belongs in this helper.

Tests must use the actual production header, plain standard C++17 and explicit failure checks that survive NDEBUG (no assert-only suite). Include: earliest preference even if later candidate is narrower; later valid candidate after malformed/overwide entries; all invalid/all overwide; exact fit; std::nextafter one representable value below/above the available width; zero/-zero; negative/NaN/+infinity/-infinity available widths and candidates; null with zero and positive counts; non-null zero count; deterministic repeated results and unmodified input. Test max finite value and matching boundary without adding widths (no overflow-prone sums). Report exact test/check counts and normal plus optimized/NDEBUG commands. No compiler installation, Actions or UE/Blender invocation. Deliver reviewed patch, baseline, byte count and SHA256 for local consumer verification.

## Local Canvas preparation and fixed semantic variants

Only skirmish row2 economy and row8 primary selection will use the helper initially. Existing title, controls, request, manual-input/focus counters, receipts, build hints, group hints and their diagnostic routes stay intact. Preserve all current row coordinates, font/scale, panel/camera exclusion and world markers. This avoids relocating the manual counters whose log correctly says countersRemainVisible=1 after bounded log suppression.

Create complete strings first, then measure every candidate using GetTextSize with the exact Font/FontScale passed to DrawText. Convert those measured widths to double and pass the current MaxTextWidth. A chosen critical string is drawn directly, never through FitToWidth. Generic hint clipping remains separate. Record selected variant/no-choice and measured widths in explicit capture-mode diagnostics. On NoChoice show a measured overflow indicator only if it fits; always mark critical-field acceptance false, even if the indicator itself cannot be drawn. No original critical text or numeric prefix may be displayed as a successful truncated value.

Root-owned variants, first preferred (cloud must not implement/change strings):

| Group | Full | Compact |
|---|---|---|
| Economy | Credits: {credits}   Delivered ore: {ore} | Credits {credits}  Ore {ore} |
| Selected primary | Selected: {count}   Primary #{id} {kind}   HP {hp}   Cargo {cargo}{rallyFull} | {count} selected  #{id} {kind}  HP {hp}  Cargo {cargo}{rallyCompact} |
| No selection | Selected: none   Primary: none | No selection |

Non-factory rally suffix is empty in both. Factory set suffix: full = '   Rally {x},{y}', compact = '  Rally {x},{y}'. Factory unset suffix: full = '   Rally none: right-click ground', compact = '  Rally none'. All numeric strings and kind names are identical across variants; retain the current integer formatting and rounded HP/rally display convention. Every selected-primary variant keeps selection count, ID, full generic kind, HP, cargo and applicable rally state; no-selection variants emit no invented zero values. Economy contains no selection suffix. Control hints already occupy separate rows; keep current input behavior and bounded manual/focus logging intact.

Local formatting/native capture cases must check identical underlying values across candidates, large values, no selection, single/group selection, each generic kind, factory rally set/unset and explicit NoChoice. Width tests alone are not proof that these strings fit the real Canvas; rendered1280/960 acceptance remains gated on a fresh allocated build and native-size inspection. Any later label/row changes need source review before accepting this scope.

## Subsequent execution

The frozen implementation passed its scoped build/native/two-viewport gates. See [scoped validation](SKIRMISH_HUD_STATUS_VALIDATION.md) for exact checkpoints and limits; those results supersede the preparation-only execution state above.
