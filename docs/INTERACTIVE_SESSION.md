# Local interactive practice

This branch advances issues #17 (bound input/focus) and #18 (HUD readability) from the tested controls source 4b07de671ab14144f7272d57f1fbea13ecef0931. Jared accepted the current visual direction for continued work, not final art approval. Existing generated models/maps/cache are reused; no art generation, new tool or roster expansion is included.

## Start and stop

After the local module is built and generated CommandProof content is present, run:

```powershell
./unreal/Scripts/start-local-interactive.ps1
```

The launcher warns for five seconds, then opens one visible 1280x720 standalone practice window. Escape exits. The default session limit is 15 minutes; it is not a persistent server. Left-click selects your green unit or clears selection on empty ground; right-click empty ground moves, right-click an enemy attacks, P produces and the wheel zooms. A produced unit starts at the original unit's position; move the selected original away to see both.

The practice map has stationary enemy targets; it is not a completed bot skirmish. Existing generic Rhino proxies and Tesla scenery remain unapproved. Process-local DisableAllScreenMessages removes engine overlay warnings while retaining the underlying log; this is presentation cleanup, not a lighting bake or shader correctness claim.

For the optional arrow-pan checks, add `-CameraPan` to that launcher. Without it, mark those checks `not-enabled`. Shift-click toggles owned live units; Ctrl+1-9 saves selection into a local group and the corresponding unmodified digit recalls it. See [group controls](GROUP_CONTROLS.md) for eligibility and queue limits. The launcher's older console hint names only groups 1-3; source at the reviewed baseline binds all nine.

## Manual checklist

Status: **not performed; physical input and real OS focus remain unverified**. This checklist was reconciled against source `49bd2e080b07476d17889f00687ffa4081ad5a60`; documentation changes do not establish a new build or session result.

1. Click the window and select your green unit; confirm its bracket and input counter change on the first click. Record whether an extra activation click was needed.
2. Click empty ground to clear, reselect, then right-click clear ground; confirm movement and a host receipt.
3. Right-click an enemy; confirm accepted attack and decreasing target HP.
4. Press P; confirm credits decrease and another unit appears. Move the original aside.
5. Shift-click the second owned unit to add it, then Shift-click it again to remove it; confirm both selection highlights and the reported selection count. Restore a known selection.
6. For each group 1-9, hold Ctrl, press and release the digit, release Ctrl, clear selection on empty ground, then press the digit alone. Confirm the saved IDs/count/primary selection return. Record each slot separately; use different selections for groups 4 and 9 and recall both again to check they stay distinct. Plain recall must not overwrite a saved group with the empty selection.
7. Scroll both ways and check zoom and counters.
8. Alt-Tab away and back; confirm the game does not force itself forward and controls work after returning. Observe the HUD's focus/foreground state and confirm no unintended orders or selection changes occur during the transition. A stale or missing observation is `unverified`, not a pass.
9. If `-CameraPan` is enabled, hold and release each arrow while the viewport is focused and foreground: confirm camera movement and no drift after release. Hold an arrow while Alt-Tabbing away, release it outside the game, then return; confirm background pan is suppressed and no stuck-key drift appears. Source permits pan only after local input setup, with both real viewport focus and foreground true, outside proof/controls automation and scripted skirmish scenarios. Do not use synthetic focus overrides as evidence for this step.
10. Press Escape to exit; confirm the session stops normally.

### Record the physical session

Keep the completed record, screenshots and relevant log excerpts together under an ignored D: evidence directory, for example `generated/manual-input-<UTC>/`. Do not overwrite historical reports. Record:

- Tester and UTC start/end; exact launch command/mode and whether pan was enabled.
- Full tested gameplay source SHA, clean/dirty source state and any diff; compiled module/build evidence and engine version/path. Record the checklist revision separately. If the binary's source identity is unknown, mark the session blocked rather than attributing it to the reviewed baseline.
- Actual viewport dimensions, window/fullscreen mode, display scaling, keyboard/mouse device descriptions, and initial/returning focus and foreground observations. Record the environment as observed; no OS setting changes or input automation are required.

Use one row per numbered step, with nine separate rows for step 6. Start every result as `unverified`; change it to `pass` only after the tester confirms the physical action and expected observation, `fail` for an observed mismatch, or `blocked` with the missing prerequisite. Use `not-enabled` only for optional pan without its flag.

| Step / group slot | Result | Physical action and observed IDs, count, receipt, camera or focus state | Evidence path / UTC and limitation |
| --- | --- | --- | --- |
| Not yet recorded | unverified | No physical session performed by this documentation update | Pending Jared's session |

Native callback/focus logs are supporting observations. They cannot identify a hardware device or prove who pressed a key. Input logs stop after 24 events and detailed focus logs after eight transitions; retain human observations and captures when those limits are reached. The HUD's native summary shows L/R/P/wheel counters; use selection highlights and request status for group outcomes rather than assuming a visible group counter.

Bound-input callback logs/counters and focus flags are observations, not proof of a particular human or OS device. The existing controls outcome scenario invokes shared handlers directly and cannot satisfy native callback counters. The separately labeled [engine-input plan](INPUT_AUTOMATION_PLAN.md) and retained [injected-digit evidence](INTEGRATION_REVIEW.md) concern engine-injected dispatch; they do not establish physical input, actual activation, capture, Alt-Tab or positive focused pan. Human verification stays pending until Jared performs and confirms the checklist. No synthetic OS input is used by this documentation update.

## Limits and validation

All tests stay local, with one heavy UE job at a time. The launcher applies a 30 FPS cap through per-process engine/game settings, two allowed CPUs/Normal priority with identity-checked polling, and soft 2048 MiB cache/1024 MiB temp guards. Polling cannot guarantee continuous confinement or cover every engine/driver write. Logs, generated content and cache remain ignored on D:. No billing/security/required-check settings or instruction files are changed.

Use `test-controls-proof.ps1 -CleanPresentation` for the existing automated outcome gates plus fresh clean-HUD captures. This remains engine automation and does not pass the manual checklist. Exact build/execution identities and results are recorded below. There is currently no evidence that Blender or UE is inadequate for this slice; the observed issues concern input and presentation rather than 3D authoring capability.
The launchers request UE's supported process-local `[AssetRegistry] EditorGameScansAR=Async` mode. Two retained runs at f697d6e timed out at frame zero while host/client synchronous asset scans were pending; the second had a 15.2-second successful standalone warmup but still exhausted the unchanged 90-second scenario deadline. Async retains asset discovery and all outcome/capture gates; it does not disable the registry or change the drive journal. Validation below distinguishes this launch fix from the compiled gameplay source.

## Verified local evidence

- Compiled gameplay/HUD source: `f697d6e90314374af7cee59586a96731f278be39`, installed UE 5.7.4 Development Editor build, 15 actions, success. Elapsed 567.14 seconds on a busy shared PC; this is not a performance measurement. No C++ changed afterward.
- Tested launch configuration: clean `2e43b186f961c2c1eb17f884bf1040d3ada6d397`. `test-controls-proof.ps1 -CleanPresentation` passed in 58.224 seconds (warmup 12.003, scenario 45.105; original scenario deadline 90 seconds). Source-reviewed async launch change retains all original gates.
- Dedicated host, rendered owner 1 and passive owner 2 exited successfully; 55 common revision digests matched. Move / Attack / Produce / Move requests each had matching accepted receipts. Bot movement, exclusive damage and expected initial/final counts passed. Initial total was five units; final total six, with five still alive.
- Two fresh 1280x720 captures were inspected. Final view shows target #3 HP 90, produced #6, selected #4 and credits 150. HUD lines are readable without engine warnings over them. Closely spaced unit labels can still overlap in the initial view; this does not approve final scale, label placement or art.
- Optimized shaders and actual 30 FPS cap were observed. Native input counters remained zero during automation, as expected. Eight harness ownership/cleanup/cache-policy cases and PowerShell parsing passed. No visible game was launched for this validation.
- Local retained report: `generated/controls-20261016-143255-124/result.json`; captures `screenshots/01-selected.png` (SHA256 `B5928D257CE28F96F03417DDD73ADE898CD8D0E096BEC4DAAD59BF43175A9A3E`) and `screenshots/02-moved-combat-produced.png` (`A0EB16C4FD88DF99AA1299AFC1C239E65F39610A063C41A1CF7434613ECE5186`). Logs/captures remain ignored and are not uploaded to Git.
- Failed synchronous-scan runs remain in `generated/controls-20261016-142321-349` and `generated/controls-20261016-143004-324`; neither was counted as a pass. The successful run had no cleanup errors and no owned UE process remained afterward.
- D: cache peaked at 37,373,710 bytes and run temp at 12,602 bytes in the passing run. The C: before/after audit began during the remaining build: seven monitored roots had unchanged file/byte totals; D3DSCache grew by two files / 32,768 bytes. On this shared PC that delta is not attributable solely to this task. This is not a zero-C-write guarantee.

Issues #17 and #18 remain open for the physical-input checklist and Jared's presentation review. This proof does not establish a completed bot skirmish, internet play, multi-player scale or production performance. The separate RPC ingress branch is not part of this change or its validation.
