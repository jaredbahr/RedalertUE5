# Reproducible blockout proof

`tools/art/build_blockout.py` authors three deliberately low-detail meshes from Blender primitives: a twin-tower Tesla Reactor interpretation, a squat tracked Rhino Tank with long cannon, and a helmeted, coat-wearing Conscript with rifle. All geometry and simple palette materials are authored by the script. No extracted meshes, sprites, textures, audio, reference images, add-ons, or third-party asset packs are used.

These are **unapproved blockouts**, not finished art or a verified likeness. Their provenance is the project brief and an authored interpretation of the requested identities; exact in-game proportions have not yet been checked against Jared's private references. Review tower arrangement, tank proportions, infantry readability, and relative scales with Jared. Do not finish meshes, textures, rigs, or effects before approval.

## Run locally on D:

From the repository root, using the verified installed Blender executable:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 4.5\blender.exe' --background --factory-startup --threads 4 --python tools/art/build_blockout.py -- --output 'D:\UserTemp\Codex-RedalertUE5-docs-20261016\generated' --render-stills --motion --motion-frames 16
```

The script has no Python dependencies beyond Blender's bundled `bpy` and standard library. Omit `--render-stills --motion` to produce assets and the editable scene without rendering; omit `--motion` for stills only. The output drive must be D:. Motion length accepts 2–48 frames, default 16. Rendering uses Workbench with material colors, studio lighting, cavity and shadows; stills are 960 × 640, motion is approximately 643 × 429 at 8 frames per second. The process is configured for at most four render threads. No physics, combat, bots, engine runtime, or network simulation runs here.

After generation, verify each FBX by reimporting it in a fresh Blender scene:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 4.5\blender.exe' --background --factory-startup --threads 4 --python tools/art/validate_exports.py -- --output 'D:\UserTemp\Codex-RedalertUE5-docs-20261016\generated'
```

This checks one mesh per file, round-trip dimensions, ground contact and zero origin, and writes `export_validation.json`. It does not replace a real Unreal import check.

Only run against the ignored `generated/` directory or a separately ignored folder on D:. Re-running replaces matching generated filenames. The source script is the reproducible deliverable; generated binaries remain local pending the Git LFS/storage budget review. Blender configuration, operating-system temporary files and driver caches may still use C:; this script alone does not verify or relocate them.

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
