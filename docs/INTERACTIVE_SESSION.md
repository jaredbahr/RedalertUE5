# Local interactive practice

This branch advances issues #17 (bound input/focus) and #18 (HUD readability) from the tested controls source 4b07de671ab14144f7272d57f1fbea13ecef0931. Jared accepted the current visual direction for continued work, not final art approval. Existing generated models/maps/cache are reused; no art generation, new tool or roster expansion is included.

## Start and stop

After the local module is built and generated CommandProof content is present, run:

```powershell
./unreal/Scripts/start-local-interactive.ps1
```

The launcher warns for five seconds, then opens one visible 1280x720 standalone practice window. Escape exits. The default session limit is 15 minutes; it is not a persistent server. Left-click selects your green unit or clears selection on empty ground; right-click empty ground moves, right-click an enemy attacks, P produces and the wheel zooms. A produced unit starts at the original unit's position; move the selected original away to see both.

The practice map has stationary enemy targets; it is not a completed bot skirmish. Existing generic Rhino proxies and Tesla scenery remain unapproved. Process-local DisableAllScreenMessages removes engine overlay warnings while retaining the underlying log; this is presentation cleanup, not a lighting bake or shader correctness claim.

## Manual checklist

1. Click the window and select your green unit; confirm its bracket and input counter change on the first click.
2. Click empty ground to clear, reselect, then right-click clear ground; confirm movement and a host receipt.
3. Right-click an enemy; confirm accepted attack and decreasing target HP.
4. Press P; confirm credits decrease and another unit appears. Move the original aside.
5. Scroll both ways and check zoom and counters.
6. Alt-Tab away and back; confirm the game does not force itself forward and controls work after returning.
7. Press Escape to exit.

Bound-input callback logs/counters and focus flags are observations, not proof of a particular human or OS device. Controls automation still invokes shared handlers directly and cannot satisfy native callback counters. Human verification stays pending until Jared performs and confirms the checklist. No synthetic OS input is used.

## Limits and validation

All tests stay local, with one heavy UE job at a time. The launcher applies a 30 FPS cap through per-process engine/game settings, two allowed CPUs/Normal priority with identity-checked polling, and soft 2048 MiB cache/1024 MiB temp guards. Polling cannot guarantee continuous confinement or cover every engine/driver write. Logs, generated content and cache remain ignored on D:. No billing/security/required-check settings or instruction files are changed.

Use `test-controls-proof.ps1 -CleanPresentation` for the existing automated outcome gates plus fresh clean-HUD captures. This remains engine automation and does not pass the manual checklist. Exact build/execution identities and results will be recorded after validation. There is currently no evidence that Blender or UE is inadequate for this slice; the observed issues concern input and presentation rather than 3D authoring capability.