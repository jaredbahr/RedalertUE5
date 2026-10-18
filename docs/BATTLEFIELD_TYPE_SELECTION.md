# Battlefield type selection with a second T press

The [Yuri's Revenge manual OCR copy](https://archive.org/stream/Command_Conquer_RED_ALERT_2_YURI_S_REVENGE/Command__Conquer_RED_ALERT_2_YURI_S_REVENGE_djvu.txt) describes T once for on-screen type selection and a double press across the battlefield. This third-party OCR copy supports that qualitative behavior. The prototype's **0.3-second inclusive interval** is an implementation choice, not a measured original-game timing claim.

The first press keeps existing visible selection behavior. A second released T press within the interval expands to owned live records of the same current generic `Kind`, including offscreen records. The original valid source anchor is retained across the first press's internal selection change and restored as primary for the battlefield result. IDs are sorted, bounded by the existing selection cap, and reserve the anchor's slot. Combat remains a generic kind, not a promise of distinct final RA2 unit types.

Held/repeated press callbacks do not count twice. Each pair is consumed; a third fresh press starts another visible-first pair. External selection/reselection, pruning, owner/epoch/mode changes, terminal skirmish state, loss of focus/foreground/viewport, invalid source records, and invalid/backward clocks cancel expansion. The internal first pass may replace an offscreen primary or select no visible records; that successful internal transition preserves the original anchor for the second press. A failed visible action cannot arm the pair. Saved control groups and authoritative state/command routes remain unchanged.

Prepared checks (no C++ compilation or UE execution yet):

- `tests/type_select_gesture_tests.cpp` includes the actual dependency-free production header, with explicit checks active under `NDEBUG`; normal/optimized execution is delegated to dot cloud.
- `test-map-type-selection.ps1` expects exactly three new `RedalertProof.MapTypeSelection.*` native groups. Existing exact Controls12 and visible-TypeSelection3 membership remain unchanged.
- `test-controls-proof.ps1 -MapTypeSelection` extends the existing controls baseline, uses a local narrow camera to keep one of two real owned matching records offscreen, and invokes the shared gesture/release path twice using actual wall time. Focus is synthetic policy input; no injected/native/physical T or OS focus claim. Existing PNG freshness/dimension/hash checks and three-role published-state/no-ingress/stationary proof remain in force. `-TypeSelection` alone remains the original visible-only fixture.
- `test-type-selection-evidence-policy.ps1` checks the unchanged default parser and its opt-in `-MapMode` variant using synthetic logs. Parser success is not engine evidence.

Follow-up to issues #17/#2; neither issue is closed by this bounded feature. No economy, ownership, balance or network authority changes. Runtime acceptance must wait for coordinated host capacity. Human trial and rollback are untouched.
