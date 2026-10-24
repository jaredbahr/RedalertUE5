# Skirmish command panel

This source candidate adds four mouse-driven actions to the generic-unit skirmish: Refinery, Factory, Combat production and Stop. A compact panel sits at the bottom right and shares its exact geometry with hit testing, edge-scroll exclusion and world-label reservations. It retains the existing information panel and host receipts.

Click Refinery or Factory, then left-click ground near the base. The first click arms placement; it cannot build beneath the button. Right-click cancels without issuing a world order. Escape also cancels before exiting an interactive session. A submitted placement leaves the mode; failed ground projection permits another location. Rematch, owner changes, focus loss and loss of required resources/structures clear pending placement.

Combat production uses the existing selected-or-first-owned factory behavior. Stop uses the existing selected-mobile-unit handler. Prices come from the command model. Disabled actions explain missing credits, HQ, factory, selection or capacity on hover. These are snapshot-derived hints: the unchanged host validator still decides whether each requested order is accepted. Scripted automation cannot issue manual commands through this panel.

Clicks over either HUD panel are consumed instead of selecting or ordering world units beneath it. The R/F/P/S hotkeys remain; R/F placement at the cursor also excludes HUD regions. Below a 640x360 viewport the new panel is hidden and existing hotkeys remain available.

The interactive launchers accept `-Viewport 1280x720` (default) or `-Viewport 960x540` for live input checks. They retain existing session limits, CPU policy, cache/temp limits and lifecycle behavior. `session.json` records the requested preset; the actual dimensions still require the engine's `PROOF_INTERACTIVE_READY` log and capture/window checks. All 77 inert checks in `test-local-interactive-paths.ps1` passed through the approved host route, including both presets, unsupported values, forwarding and existing path/lifecycle invariants. The earlier restricted-route attempt failed with access denied and remains recorded; neither attempt launched Unreal.

## Validation boundaries

The production header passed **229 checks in each of two MSVC C++17 builds**, normal and optimized `/O2 /DNDEBUG`, with `/W4 /WX` and zero failures. The standalone test is `tests/command_card_tests.cpp`. It covers shared geometry at four viewport sizes, invalid inputs, complete button boundaries, credit and capacity edges, prerequisites, placement/submission/cancellation, HUD click consumption and context changes. It does not load Unreal, render fonts, submit an RPC or establish physical input behavior. The Unreal adapter's focus and viewport API calls were reviewed as source only.

For an existing MSVC developer environment, create the ignored `generated/` directory, then compile and run:

```text
cl /nologo /std:c++17 /EHsc /W4 /WX /Iunreal\Source\RedalertProof tests\command_card_tests.cpp /Fogenerated\command-card-tests.obj /Fegenerated\command-card-tests.exe
generated\command-card-tests.exe
```

Repeat with `/O2 /DNDEBUG` and distinct output names to verify explicit checks remain active in an optimized build.

Unreal compilation and rendered validation are still pending. The next allocated engine batch must inspect complete labels/prices and world-label exclusions at 1280x720 and 960x540, then exercise clicks, hotkeys, rejected orders, cancellation, focus loss and rematch with actual host receipts. Existing functional deadlines, cleanup policy and PR71 acceptance remain unchanged. No diagnostic retry, new art, binary upload or prototype record-cap increase is part of this patch. Issue #18's selected-world-label and dense-scene gaps remain open; this panel can reserve additional world space, so the earlier captures cannot establish this candidate's readability.

See the [bounded engine and input validation plan](COMMAND_CARD_VALIDATION.md) for payload commands, admission, resource limits and remaining controller/review prerequisites.
