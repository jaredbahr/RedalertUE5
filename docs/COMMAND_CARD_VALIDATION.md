# Command-panel engine validation plan

Preparation only: no Unreal build, rendered run or live-input pass has been recorded for this candidate. Standalone C++ checks and inert launcher checks are separate evidence. PR72 remains draft until actual engine/input validation and independent review succeed. PR71's frozen sources, failed receipts, qualification rules and consumed diagnostic restriction remain unchanged.

## Admission and resource request

Run serially in an isolated source checkout on designated storage. Obtain current coordinator confirmation that higher-priority test lanes are drained, then fresh host/process evidence showing no active game or competing validator. Do not stop them. Use the existing 20-second admission bound and a qualifying 600-second quiet window, with CPU below 80%, at least 8 GiB physical memory free, at least 20 GiB storage free and no sample gap above 10 seconds. Recheck freshness at dispatch and abort when capacity or the reservation is lost.

Reserve two logical CPUs at affinity `0x44`, Normal priority, one compile action and no UBA. Verify owned-child affinity; inherited settings and polling are not a hard job-wide CPU quota. The 8 GiB free-memory threshold is a planning envelope, not a measured requirement for this revision. An older single-editor demo observed about 2.02 GiB peak working set; the new build and host/client peaks are unknown and must be recorded. Keep cache/temp output on designated storage. Rendered/interactive launchers retain their existing 2 GiB cache and 1 GiB temp soft limits; no hard filesystem quota is claimed. Do not infer zero system-drive writes or a measured frame rate from configuration.

Use separate, unextended 20-minute reservations for build, rendered validation and human input as needed. Release the slot between reservations. Do not fit extra work by extending a failed deadline or lease.

| Stage | Existing bound used by this plan |
| --- | --- |
| Editor module build | 900 seconds, plus one aggregate 5-second owned cleanup budget |
| HUD native automation | 90 seconds inner, 120 seconds enclosing stage, 5 seconds cleanup |
| Each two-viewport Victory/capture run | 220 seconds enclosing stage, 5 seconds cleanup |
| Each interactive input run | 180-second session, 220-second enclosing stage including setup, 5 seconds cleanup |

The commands below are **payloads**, not permission to run an unsupervised launcher. The build script has no overall deadline, and the interactive script alone does not kernel-confirm final cleanup. A reviewed PR72 binding to an owned Windows Job supervisor must create the exact child suspended, set affinity before resume, enforce the listed deadlines, retain PID/creation/executable identity, capture bounded output and confirm zero survivors. Any timeout, forced cleanup, missing identity or incomplete output fails the stage. Do not retarget PR71's acceptance drivers or present their earlier results as PR72 validation. The PR72-specific controller/binding review is still pending.

## Build and provenance

Bind a clean exact source revision, C++ tree, Config/project inputs, installed engine and all reused content hashes before execution. Reuse only the existing authored blockouts and maps; no generation or binary upload is needed. Begin without an existing candidate DLL. Under the bounded controller:

```powershell
& .\unreal\Scripts\build-native-proof.ps1 -EngineRoot $env:UE_ENGINE_ROOT
```

Require actual UBT success, zero exit, complete supervisor/cleanup outcome and a fresh DLL plus `UnrealEditor.modules`. Record their SHA256, BuildId, source/config identities and creation/modification times inside the build interval. A compatible BuildId or an older DLL alone cannot establish provenance.

## Native and rendered checks

Use that exact built DLL and content throughout these serial payloads:

```powershell
& .\unreal\Scripts\test-hud-layout-model.ps1 -EngineRoot $env:UE_ENGINE_ROOT -TimeoutSeconds 90
& .\unreal\Scripts\test-small-match.ps1 -EngineRoot $env:UE_ENGINE_ROOT -Scenario Victory -TimeoutSeconds 220 -CleanPresentation -ReadabilityCaptures -CameraEdgeCaptures -Viewport 1280x720
& .\unreal\Scripts\test-small-match.ps1 -EngineRoot $env:UE_ENGINE_ROOT -Scenario Victory -TimeoutSeconds 220 -CleanPresentation -ReadabilityCaptures -CameraEdgeCaptures -Viewport 960x540
```

Require the existing exact thirteen HUD native successes with zero failures. Each Victory run must retain its original assertions and complete actual host/bot/result receipts. Verify fresh PNG hashes/dimensions and inspect native-size captures for all four command buttons, full keys/prices, contrast, overflow indicators, panel/marker exclusions and the existing primary-label limitations. Earlier captures cannot validate the new reserved panel. These scripted terminal-scene captures do not demonstrate active placement, physical clicks or focus behavior.

## Live input at both presets

With a tester ready, run each preset separately under its owned controller:

```powershell
& .\unreal\Scripts\start-small-match.ps1 -EngineRoot $env:UE_ENGINE_ROOT -StorageRoot $env:RA2_STORAGE_ROOT -SessionSeconds 180 -Viewport 1280x720 -CameraPan -CameraEdgePan
& .\unreal\Scripts\start-small-match.ps1 -EngineRoot $env:UE_ENGINE_ROOT -StorageRoot $env:RA2_STORAGE_ROOT -SessionSeconds 180 -Viewport 960x540 -CameraPan -CameraEdgePan
```

Record actual `PROOF_INTERACTIVE_READY` dimensions, source/DLL identity, window/load state, log receipts and the tester's observations. Verify: initial selection and keyboard R/F/P/S behavior; enabled/disabled prices and prerequisite hints; button-only arming without construction; valid ground placement and host rejection of an invalid location; right-click/Escape cancellation; factory production; selected-mobile Stop; no order or selection click-through on either HUD panel; no edge scrolling over the command panel; focus-loss cancellation; terminal disabling and rematch reset when reached. Compare local-owner commands and resource events, allowing legitimate mining/bot activity; an unchanged whole-match digest is not a valid cancellation criterion.

The session must exit normally within its bound, without an expired/error receipt or owned survivor. A partial checklist stays partial: defer unreached cases to a distinct bounded case rather than stretching the session or retrying until pass. Script counters stay `physicalInputVerified=false`; a tester's explicit physical-input/focus confirmation is a separate record. This is local skirmish validation, not Internet, army-scale, final-art or PR71 acceptance.
