# Selected world-label placement

Related to issue #18. This change belongs to the existing HUD coordinator and
builds on observer source `1c057572bbba05dca6beec69a3944c2332fefb1b`.
It is separate from PR #72's terminal/export repair and selected-glyph draw order,
and from PR #79's input integration. The observer branch and captured evidence
remain historical inputs; this is not evidence that the observer caused a regression.

## Observed failure

At observer `1c057572bbba05dca6beec69a3944c2332fefb1b`, clean 1280x720
Victory captures omitted the selected C#32
world ID/HP group in `03-left-edge.png`, `05-panel-edge.png` and
`06-camera-before.png`. The matching complete placement rows report priority 0,
`placed=false`. The latter two frames have identical relevant reservations.
The clean 960x540 run selected C#30 at different positions; its success does not
invalidate these failures or establish a causal effect of the capture flags.

The existing primary path searches around an above-glyph preference. Both
recorded failures have a clear below-glyph preference. This motivates a second
bounded search; it does not prove why every original candidate was rejected.

## Change and limits

Primary ID/HP groups try the existing above-glyph placement first. If it fails,
they try below the same glyph through the same linked-layout primitive. Ordinary
units, resources and legacy labels retain their existing placement paths.
The primary still places before lower-priority label groups.

Each pass retains the primitive's 66-candidate bound, viewport inset, rectangle
separation, exact source-token identity, foreign-glyph exclusion and leader
collision checks. Failed passes reserve nothing; if both fail, the whole group
is omitted. There is no partial text/HP rendering or obstacle exemption.

The primary wrapper permits at most **132 candidates across two anchors**.
The displacement limit is relative to each anchor. In the captured cases the
below preference is 69 pixels from the original above preference, so the old
single-origin 64-pixel bound must not describe the combined search. Capture
schema v2 records the chosen/last preference, strategy, actual passes, per-pass
candidate cap and combined candidate cap for accurate replay.

## Regression and pending acceptance

Two new native tests retain the complete ordered reservations and exact doubles
for the two captured geometries, plus invalid/offscreen, source-identity,
priority, determinism and failed-placement atomicity controls. The HUD runner
requires all original 13 tests and both new names, for 15 total; its timeout,
CPU policy and cleanup checks are unchanged.

**Native tests, compilation and rendered validation of this fix are unrun.**
Imagination owns the heavy slot. Prior observer build/13-test/capture passes do
not validate this changed source. When admitted, build and execute the exact
15-test manifest, inspect both viewport capture sets and preserve all failed
baseline rows. Actual input/focus, terminal/rematch and app-control acceptance
remain separate; the timed-out approval is not retried by this change.
