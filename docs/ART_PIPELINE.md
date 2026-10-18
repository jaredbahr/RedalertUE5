# Reproducible blockout proof

`tools/art/build_blockout.py` authors three deliberately low-detail meshes from Blender primitives: a twin-tower Tesla Reactor interpretation, a squat tracked Rhino Tank with long cannon, and a helmeted, coat-wearing Conscript with rifle. All geometry and simple palette materials are authored by the script. No extracted meshes, sprites, textures, audio, reference images, add-ons, or third-party asset packs are used.

These are **unapproved blockouts**, not finished art or a verified likeness. Their provenance is the project brief and an authored interpretation of the requested identities; exact in-game proportions have not yet been checked against Jared's private references. Review tower arrangement, tank proportions, infantry readability, and relative scales with Jared. Do not finish meshes, textures, rigs, or effects before approval.

## Run locally on the designated non-system drive

From the repository root, set the process-local `BLENDER_EXE` environment variable to the installed Blender executable. Then run:

```powershell
& $env:BLENDER_EXE --background --factory-startup --threads 4 --python tools/art/build_blockout.py -- --output (Join-Path $PWD 'generated') --render-stills --motion --motion-frames 16
```

The script has no Python dependencies beyond Blender's bundled `bpy` and standard library. Omit `--render-stills --motion` to produce assets and the editable scene without rendering; omit `--motion` for stills only. The output drive must be the designated non-system drive. Motion length accepts 2–48 frames, default 16. Rendering uses Workbench with material colors, studio lighting, cavity and shadows; stills are 960 × 640, motion is approximately 643 × 429 at 8 frames per second. The process is configured for at most four render threads. No physics, combat, bots, engine runtime, or network simulation runs here.

After generation, verify each FBX by reimporting it in a fresh Blender scene:

```powershell
& $env:BLENDER_EXE --background --factory-startup --threads 4 --python tools/art/validate_exports.py -- --output (Join-Path $PWD 'generated')
```

This checks one mesh per file, round-trip dimensions, ground contact and zero origin, and writes `export_validation.json`. It does not replace a real Unreal import check.

Only run against the ignored `generated/` directory or a separately ignored folder on the designated non-system drive. Re-running replaces matching generated filenames. The source script is the reproducible deliverable; generated binaries remain local pending the Git LFS/storage budget review. Blender configuration, operating-system temporary files and driver caches may still use the system drive; this script alone does not verify or relocate them.

## Outputs and import contract

| Local generated output | Purpose |
| --- | --- |
| `RA2_Blockout_Review.blend` | Editable grouped scene, review camera and placeholder motion |
| `assets/SM_TeslaReactor_Blockout.fbx` | One combined static mesh, origin at ground center |
| `assets/SM_RhinoTank_Blockout.fbx` | One combined static mesh, origin at ground center |
| `assets/SM_Conscript_Blockout.fbx` | One combined static mesh, origin at ground center |
| `previews/rts_normal.png` | Normal RTS group view |
| `previews/rts_wide.png` | Zoomed-out group view |
| `previews/motion_001.png` onward | Short placeholder motion sequence |
| `previews/review.html` | Offline play/pause viewer for the motion sequence and still links |
| `blockout_manifest.json` | Actual Blender version, exported dimensions/vertex counts and camera/motion metadata |

Blender uses meters (`scale_length=1`), +Z up and local -Y forward. FBX export uses -Y forward/+Z up and unit metadata; Unreal import should enable scene and unit conversion. Expected approximate sanity checks are a 5.45 m reactor including terminals, a 6.2 m overall tank including gun, and a 1.95 m Conscript. Actual dimensions are recorded in the manifest, not treated as approved gameplay scale. Check imported dimensions in centimeters before treating the conversion as validated. No collision, LOD, skeleton, gameplay socket or finalized material contract is supplied.

The normal camera is orthographic with a 24 m horizontal field, located at `(18, -24, 22)` m looking at `(0, 0, 1.3)` m. The wide view uses 36 m. Motion translates and turns the tank, translates/bobs the whole infantry blockout and pans/zooms the camera. It is a visual review aid, not a walking rig or gameplay proof. The scene opens at the first frame so still comparisons are repeatable.

## Review gate

Inspect both stills and the motion sequence together. Record Jared's approval or requested changes to silhouettes, faction character, proportions, relative scale and RTS recognition before finishing assets. Keep the baseline captures for comparison. Successful image generation establishes only that this local content pipeline ran; it establishes no Unreal compatibility, art approval, performance budget, multiplayer correctness, or large-army scalability.

## Evidence identity and failure behavior

New runs assign a UUID `run_id`, SHA-256 of the pipeline Python sources, and SHA-256 of each exported FBX. The manifest records signed local bounds as well as dimensions. Captures list their own hashes under that same run identity; `stills` and `motion` explicitly read `omitted` when not requested. The editable scene has its own hash. These identify actual bytes; they are not art approval or a claim of deterministic FBX encoding.

Before generation, only pipeline-owned `previews/`, the old manifest, and the export/import/probe reports move into `generated/evidence_history/<superseding-run-id>/`, marked historical by `ARCHIVED.json`. This preserves prior review captures rather than presenting them as captures of new geometry. Separate user baseline/delivery folders are untouched. The current reports begin as `passed: false`, `status: not_run`. Each validator invalidates its current success report **before** reading/checking inputs; an exception leaves `checking_or_failed`, never a stale current pass. Every UE import also invalidates prior probe success, including same-run attempts. Validation refuses source or FBX hash mismatches and incomplete generation. Old manifests require regeneration, not compatibility guessing.

Set the process-local `PYTHON_EXE` environment variable to a compatible installed Python interpreter. Run the inexpensive numerical/provenance regressions without Blender:

```powershell
& $env:PYTHON_EXE -m unittest discover -s tools/art -p test_evidence.py -v
```

The three tests independently encode asymmetric bounds and reject 10x uniform scale, X-only scale, 100 cm mesh displacement, displaced pivot, reversed facing and incorrect actor basis. A temporary-output regression preserves an old capture and successful reports, changes source/geometry, omits new captures, then forces an export-hash failure; current reports must remain false while archived evidence remains readable. Another executes the actual validator/importer entrypoints with malformed manifests and inert engine modules, requiring previous success reports to become false before parsing fails. These are pure Python gates; the engine probe below is separate evidence.

Engine smoke acceptance remains: regenerate on this source; validate FBXs; import fresh/repeat in UE; match current run/hash metadata; check all XYZ dimensions, signed local bounds, actor pivot, ground and basis. Do not use a previous branch's successful reports as proof for the changed gates.
