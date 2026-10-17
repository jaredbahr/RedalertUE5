# UE visual proof setup

## Verified tools, selected baseline

Read-only inspection found `D:\Epic Games\UE_5.7\Engine\Build\Build.version`: **UE 5.7.4**, changelist **51494982**. Both `UnrealEditor.exe` and `UnrealEditor-Cmd.exe`, the bundled Python runtime, and PythonScriptPlugin are present. The `UE_5.8` directory has no `Build.version`; do not select it as a working installation based on its folder name. VS Community 2022 was found via vswhere at `D:\DevTools\VisualStudio\2022\Community` (17.14.37710.0), and Windows SDK include version `10.0.26100.0` is installed. MSVC `14.44.35207/bin/Hostx64/x64/cl.exe` is present at that VS installation. Presence is not a successful compile. This content-only proof requires no C++ compilation or additional installs.

`RedalertProof.uproject` enables the installed PythonScriptPlugin and EditorScriptingUtilities only. The bundled AndroidFileServer plugin is explicitly disabled for this project to avoid unrelated file-server settings being generated. No third-party dependencies or extracted game content are included. Generated FBX, imported materials, meshes and level stay ignored locally; inspect binary/LFS allowance before any later upload.

## Regenerate

First run the repository's Blender blockout generator, producing:

- `generated/assets/SM_TeslaReactor_Blockout.fbx`
- `generated/assets/SM_RhinoTank_Blockout.fbx`
- `generated/assets/SM_Conscript_Blockout.fbx`

The art pipeline exports Blender meters with FBX scene-unit metadata. Import uses scene and unit conversion, combines each file into one static mesh, and reports full bounds in **UE centimeters**. Expected tank length is approximately 6.2 m, reactor height 5.45 m, and infantry height 1.95 m; verify actual dimensions in `generated/ue_import_report.json` before trusting scale.

From the repository root in PowerShell:

```powershell
# Root schedules this CPU-heavy smoke run after Blender completes.
& .\unreal\Scripts\launch-proof.ps1 -Import -NullRHI
# Open the saved level with graphics for inspection after a successful import.
& .\unreal\Scripts\launch-proof.ps1
```

Each import task must report the exact expected mesh object path and produce an actual StaticMesh before scene construction, preventing a stale prior asset from masking a failed reimport. Success requires process exit 0, `REDALERT_PROOF_OK` in output, all three plausible mesh bounds in the report, and a saved `unreal/Content/BlockoutGenerated/Maps/VisualProof.umap`. Rerun the import and confirm it does not duplicate tagged scene actors. The script only imports under `/Game/BlockoutGenerated`, creates/loads its own generated map, and rebuilds actors tagged `RedalertProofGenerated`. Keep authored work outside that namespace. It does not delete other content or maps. Reimport intentionally replaces generated meshes/materials there; do not hand-edit them as source art.

The level arranges all three meshes, ground, directional light, fill light, and two labeled orthographic cameras: `RTS_Normal` (2500 cm width) and `RTS_ZoomOut` (4500 cm width), both at `(0,-2200,2200)` cm with pitch -45 and yaw 90 degrees, 16:9. Pilot each camera in the editor for repeatable inspection. These settings are proposed review views, not approved gameplay settings. UE motion capture is not implemented; the source Blender preview supplies placeholder movement for the blockout review.

## Commandlet import compatibility

The first UE 5.7.4 headless smoke failed with exit 3 while importing through **Interchange**: `CurrentApplication.IsValid()` in Slate, after an Interchange/ContentBrowser callback. The importer now supplies an explicit `unreal.FbxFactory()` for every automated task. In the installed engine source, `Engine/Source/Developer/AssetTools/Private/AssetTools.cpp` selects Interchange only when `SpecifiedFactory == nullptr` (around lines 3522–3525); `FbxFactory.cpp` consumes the task's `FbxImportUI` options and suppresses the options dialog for automated imports. This uses the installed conventional FBX importer without changing engine-wide settings or disabling unrelated formats.

A first retry after that failed Interchange run should start with a clean **generated** `unreal/Content/BlockoutGenerated` folder, because partially imported Interchange assets may retain a different reimport handler. Preserve any authored work elsewhere. The launcher does not erase content automatically. A successful fresh import and successful rerun are still required before calling this remedy verified.

## D: paths and evidence boundaries

The launcher limits UE-reported cores to four (`-CoreLimit=4`), disables sound, writes `generated/ue-proof.log`, requires a D: checkout and applies **process-local** DDC and TEMP/TMP environment variables, restoring them when it exits. `-ddc=BlockoutDDC` selects the project's explicit filesystem-only cache graph, with `generated/ue-cache` as its environment override and shared cache disabled. Temporary files target `generated/ue-temp`; project Saved/Intermediate/Content stay under the D: project. No global registry, engine installation configuration, security setting, or machine environment variable is changed.

These are configured paths, not proof that every engine/plugin/Windows operation avoids C:. A smoke run must inspect the actual log for active DDC nodes/paths, temp/output locations, failures and residual C: access. This custom graph avoids selecting Zen for this invocation; do not relocate or modify a separately running Zen server. Do not claim cache verification merely because D: directories exist.

## Approval and limits

Review normal RTS and zoom-out stills plus placeholder motion together. Jared must approve silhouettes, proportions, relative scale and readability before finishing meshes, textures, rigs or effects. No approval is recorded. This scene provides no selection, gameplay, bots, network replication, internet connectivity, eight-player result or large-army benchmark. Technical gates remain in [MILESTONES.md](MILESTONES.md).



## Executed smoke evidence

Fresh and repeat commandlet imports ran successfully with installed UE 5.7.4 using NullRHI, emitted REDALERT_PROOF_OK, saved the level and produced expected centimeter bounds. The importer also asserts and reports exactly eight tagged generated actors with expected labels to detect duplicate regeneration. See [First-proof evidence](FIRST_PROOF.md) for actual measurements and limits. Engine logs confirmed writable D: DDC and shader-temp paths; this does not prove all plugin/OS writes avoid C:.

## Strict geometry and reimport smoke procedure

The importer now requires a complete current manifest and matching source/FBX hashes. Every XYZ dimension must match manifest meters times 100 within the greater of 0.5 cm or 0.1%; signed lower/upper local bounds, zero-height ground contact and actor pivot use 0.5 cm tolerance. Actor local -Y/+Z basis is checked separately (0.001 component tolerance) so equal-size rotated actors cannot pass solely on dimensions. FBX handedness conversion is explicitly expected as source `(X, Y, Z)` to UE `(X, -Y, Z)`; source -Y semantic forward therefore maps to UE +Y mesh geometry. This signed mapping must pass the scheduled real import before being called verified. The report names these actor_minus_y_world and actor_plus_z_world explicitly; neither is named mesh forward. Actor basis and mesh direction are separate checks: signed asymmetric bounds can detect some reversed geometry, but neither bounds nor actor rotation establishes visual semantic nose direction. Confirm that direction by inspecting the imported cannon/face in the scheduled smoke. No geometry is polished or rotated to hide failures.

The report records run/source/asset identity, bounds, pivot, actor basis and material-slot names, and starts with `passed: false` before checks. A failed run cannot leave its predecessor's report as current success. Fresh/repeat engine execution passed for the changed gates as recorded below; the earlier smoke describes the previous validator.

Repeat-import preservation and remaining negative checks:

1. After fresh import, add an **untagged** sentinel actor to the generated map and save; create a separate test asset outside `/Game/BlockoutGenerated` with a generic palette name such as `Red`. Record their object paths and sentinel transform.
2. Repeat import; assert both sentinel and external asset still exist with unchanged identity/properties, eight tagged actors remain, and no imported material references resolve outside the reserved namespace. Generic names must not overwrite external content.
3. Record mesh material-slot count/names and material object paths after fresh/repeat import. Change a palette color in an isolated test copy of the source, regenerate (new identity) and reimport; inspect referenced material color values and rendered appearance. Do not assume `replace_existing` updates material colors: if it does not, report/fix that gate before claiming color reimport works. Restore the source, regenerate and reimport afterward; preserve the earlier captures as history.
4. Feed independent 10x scale, X-only scale, 100 cm mesh Z offset and wrong-facing artifacts in a separate ignored test output. Each must fail the corresponding gate and leave `passed: false`. The lightweight fixture tests cover rejection logic; real importer negative fixtures remain a separate execution gate.

Use a disposable generated scene for these probes. Do not erase unrelated content or approved baselines. Only root schedules heavy Blender/UE runs alongside the other project's workload.

`unreal/Scripts/probe_asset_evidence.py` supplies a bounded post-import engine probe for root's schedule. It creates a probe-tagged sentinel (without the generator tag) and a metadata-marked duplicated engine cube at `/Game/AssetEvidenceProbe/ExternalCubeSentinel`; collisions with unrelated actors/assets abort. It repeats the importer, requires sentinel identity/location/mesh preservation, identical external `.uasset` SHA-256, and eight generated actors. Actual UE tank actor transforms are mutated to 10x scale, X-only scale, +100 cm Z and 180-degree facing, each requiring rejection; `finally` restores the actor. Results go to ignored `generated/ue_asset_probe.json`. This does not exercise edited FBX vertices/pivots, material-color updates, generic material-name collisions or visually prove mesh semantic front; the report explicitly lists these as unverified. Fresh and repeat execution passed on installed UE 5.7.4. No cleanup/deletion is performed.

Observed smoke: geometry-only Blender generation, three FBX round trips, fresh/repeat UE import and the preservation/mutation probe passed. The yaw-only case records pitch=0, yaw=180, roll=0 and is rejected by signed Y bounds. The other rejected actor mutations are uniform 10x, X-only 2x and +100 cm Z. Eight generated actors and 14 material slots remain; the external package hash is unchanged. The separate pure Python tests exercise numerical mesh/pivot gates; actual corrupt FBX-local vertices/pivots remain untested. No current captures are claimed for this geometry-only run.

Final reviewed-source run: `75dd642eed1646888033b1bc54b58efa`. The manifest, `export_validation.json`, `ue_import_report.json` and `ue_asset_probe.json` share its identity under ignored `generated/`; current source hash is `24fa18558b6f6b1ca433e7e87631919400e64ad9217d820ec4257b70e8662fd6`. An independent reviewer checked source and report freshness. Prior runs remain labeled historical.
