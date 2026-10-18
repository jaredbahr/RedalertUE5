# Try the local bot skirmish

The smallest trial is an editor-backed standalone game with construction, harvesting, combat, a bot opponent, victory/defeat and rematch. Scripted matches have passed. A complete match using physical keyboard/mouse, focus recovery and the human N rematch callback still need acceptance. This is a prototype with proxy art and placeholder rules; RA2/Yuri gameplay fidelity remains the goal.

## Prepared PC launch

Use a prepared checkout containing the tested DLL and generated map. A fresh Git clone contains neither. Obtain the prepared-checkout location privately from the maintainer; personal machine paths are deliberately omitted here.

Set the process-local environment variable `UE_ENGINE_ROOT` to the installed engine root, and `RA2_STORAGE_ROOT` to an existing designated non-system directory containing the prepared checkout. Open PowerShell at the prepared repository root and run:

```powershell
.\unreal\Scripts\start-small-match.ps1 -EngineRoot $env:UE_ENGINE_ROOT -SessionSeconds 900 -CameraPan -CameraEdgePan
```

Wait until the shared-PC engine allocation is free before launching. The script warns for five seconds, then opens a visible 1280x720 game window. Click inside it to focus. Startup has varied; do not mistake a slow startup for a completed input test or start duplicate instances.

The updated launcher accepts explicit `-EngineRoot` and `-StorageRoot` overrides. It rejects missing configuration, system-volume storage, a checkout outside the configured storage root, and junction/reparse paths before creating output or launching a process. Keep these settings in the current PowerShell process; do not commit personal paths or change machine-wide environment settings. This launcher-only change does not validate a new engine build or physical input.

This uses installed UE 5.7.4 and existing local assets. It is not a packaged executable. The default guard closes the session after 15 minutes, including any rematches; Escape exits earlier. The script limits its game process to two available logical CPUs at Normal priority and targets 30 FPS, without proving measured frame rate. Process cache/temp paths point into this checkout's ignored `generated/`; this is not an exhaustive guarantee of zero writes to the system drive. Sound is disabled.

## Controls and first match

| Action | Control |
| --- | --- |
| Select an owned unit/building | Left click |
| Add/remove an owned unit from selection | Shift + left click |
| Move selected mobile units | Right click ground |
| Attack with selected Combat units | Right click an enemy |
| Build refinery / factory near your HQ | Point at clear ground, press R / F |
| Gather ore | Select Harvester, right click an ore marker |
| Produce a Combat unit | Select owned factory, press P |
| Set factory rally | Select owned factory, right click ground |
| Stop selected mobile units | S |
| Select visible owned units of the selected generic kind | T |
| Save / recall selection group | Ctrl + 1–9 / 1–9 |
| Attack Move for selected Combat units | Ctrl + Shift + right click ground |
| Zoom | Mouse wheel |
| Optional camera pan / edge scroll | Arrow keys / outer 16 viewport pixels |
| Rematch after a terminal result | N, as active owner 1 |
| Exit | Escape |

Start by placing a refinery and factory on clear ground close to your HQ, separated from other buildings. Select the Harvester and order it onto ore; watch credits replenish on delivery. Select the factory, set a rally point and press P to produce combat units. Select those units and right click enemy units or the enemy HQ. Destroy the enemy HQ to win, or lose your HQ to reach defeat. After the result, press N to restart with fresh starting state.

Builds require credits and valid placement; production also has a small live-record cap. Read the HUD's request/receipt status when an action is rejected. Costs, instant construction/production, generic unit kinds and the cap are engineering defaults, not original-game balance. Attack Move acquires only within weapon range and does not pursue outside it. Edge pan pauses over the HUD, while a mouse button is held, or without viewport focus/foreground.

## Smallest outstanding human check

Allow a focused 15–30 minute window once the PC is free; this is a planning allowance, not a completion guarantee.

1. Launch once. Confirm the real mouse selects and orders movement; press S while moving, and wheel both ways.
2. Build R/F, gather ore and produce with P. Confirm HUD feedback and visible state changes.
3. Alt+Tab away while a pan key/edge condition is active. Confirm the game stops panning while unfocused and resumes normally after clicking back.
4. Play to one natural victory or defeat. Press N and confirm starting assets, selection and economy reset; issue a fresh order.
5. Escape and confirm the owned game process exits. Record any missed controls, stuck focus, rejection or startup failure.

Pass requires observed physical actions and a full terminal/rematch cycle. Runtime scripts and callback injection cannot substitute for this observation. Physical acceptance remains pending until a human performs this check or suitable observation/input tooling becomes available.

The launcher prints an ignored `generated/<interactive-run>/session.json` path; its sibling `game.log` records the session. Preserve both on failure. The receipt deliberately keeps `physicalInputVerified=false` and `humanConfirmationRequired=true`: attach human observations separately rather than changing those fields to imply automated proof. If a required local file is missing, stop and report its path; do not install or regenerate assets just to follow this guide.

## Tested source and queued work

The prepared build's engine inputs match main at `4d9022339b249c9282a4146385f01a19057b787e`: Source `3e2d535030ef332108077f12c9cb3e9f3c312b1f`, Config `41ebcbe583f58912eaf58951ad071addcab503db`, project `176dd85ae0a2ca9667b749d2d2592a500de68ccf`. Its local DLL and generated-asset hashes were checked against retained private evidence. Recheck that binding before reuse after source changes; an old DLL does not validate new source.

See [player-feature evidence](PLAYER_FEATURES.md), [lifecycle evidence](SESSION_LIFECYCLE.md) and [input automation limits](INPUT_AUTOMATION_PLAN.md) for scoped results. Exact local paths, raw logs, session identifiers and machine details stay in ignored local evidence.

[PR45](https://github.com/jaredbahr/RedalertUE5/pull/45) separately combines native Stop coverage and cooperative context orders. Its combined UE acceptance is pending; standalone evidence does not validate the combined build. The separate legacy HUD label repair also awaits compilation/rendered review. Neither is required to try this current-main checkpoint. Internet play, full factions, practical army budgets and finished art remain separate gates.
