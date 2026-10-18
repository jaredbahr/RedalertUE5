# Mesh-specific unit-view grounding

## Measured problem

An isolated Harvester view established a display placement error. Its engine cube had local bounds `[-50,+50]` cm, component scale approximately `(1,0.7,0.7)`, zero rotation and center Z=0. The resulting lower bound was approximately **-35 cm**, while the measured saved floor's top was Z=0. Original contact pixels showed the intersection.

This before evidence is explicitly **partial observations from a failed diagnostic**, not a completed runtime acceptance. It came from diagnostic `14a9abf88e7fe7e74d94b16309b6b05d9dd09a83` on main `e5f4703c1b274663fe7e3795b3dd238bb6c52f69`; only two of five required captures completed. The supplied source patch reconstructs source tree `691c1857f38a01ffa5d19929cd377ed87a4b55b9`. The observations are not from the later repaired observer binary.

Before-image SHA-256 identities:

- `03-geometry-harvester-contact.png`: `b59db138fea1e5ae0aac92e82c2414afbca72191b7cf60af0751ce7201ed1680`
- `04-geometry-harvester-motion-a.png`: `311865ae6d047d16a787eac610e99aef6dc15553146dbd29857b73cd6959c3e0`

Measurements were sampled at screenshot request while simulation continued. The pixels and transforms establish the placement defect, but no exact sample/render-frame lock, full runtime pass, Rhino facing, physical-input, performance, balance or art approval is claimed. Same-unit motion of approximately 205 cm is recorded numerically; the camera also moved, so these stills alone do not measure its trajectory.

## Change

The runtime previously used a global “Rhino loaded” flag to choose every unit view's center offset: zero for a loaded blockout, otherwise +30 cm. A Harvester can select a centered cube even while the Rhino loaded successfully, so that global choice was unrelated to its actual mesh pivot and height.

The new placement uses the **mesh actually selected by the existing binding logic**, its local bounds, and its current component rotation and scale. The transformed local minimum Z is computed from all three scaled local axes. The component origin is placed at authoritative `Unit.Position` plus only the offset needed to bring that lower bound to the model ground plane.

- Centered cubes receive their own scaled lower-bound offset, including the measured Harvester's approximately +35 cm.
- Ground-origin meshes stay grounded without a cube-specific offset.
- Rotations, nonuniform or signed scale, and arbitrary local pivots are represented by the full transform's scaled axes. Existing translation does not enter the offset, preventing cumulative lift.
- Mesh selection, asset bytes, display scales, authoritative positions, collision, gameplay, camera defaults and heading behavior are unchanged.

This is not terrain following. The existing model position remains the ground-plane contract; its Z=0 plane matches the measured current floor. An independently displaced or sloped map would need a separately defined terrain-placement contract.

## Independent visual assertion

The readability camera-rig check no longer repeats the obsolete 0/+30 cm center expectation. It obtains the selected mesh's actual world bounding box after the rig move and a later tick, and requires:

1. A valid view, mesh and world bounding box.
2. Component X/Y equal to authoritative unit X/Y within the existing 0.01 cm tolerance.
3. Actual world lower Z equal to authoritative ground Z within that tolerance.

This uses Unreal's box transformation independently of the production offset helper. A wrong placement formula must fail the rendered assertion rather than change its expected answer to match itself.

## Regression and acceptance

The native test is exactly `RedalertProof.Controls.UnitViewGrounding`. The exact-name manifest in `unreal/Scripts/test-group-controls.ps1` includes it, taking that group from 14 to 15 names. Other groups and admission/runtime budgets are unchanged.

Portable coverage exercises the production numeric helper against independent eight-corner bounds calculations and known expectations. Native coverage uses Unreal's `FBox`/`FTransform` to check actual engine math. These tests cover grounded and centered pivots, existing display scales, rotation, signed/nonuniform scale, nonzero ground Z and repeated placement. Synthetic bounds/fallback cases do not claim to load missing assets or execute Unreal on the cloud host.

Portable commands from the repository root:

```sh
bash tests/run_unit_view_grounding_tests.sh
bash tests/run_unit_view_grounding_tests.sh --sanitized
```

The default command runs both normal and optimized builds; `--sanitized` runs AddressSanitizer and UndefinedBehaviorSanitizer with leak detection disabled.

## Local acceptance

The exact production checkpoint `ed7bf581e2a698bf827b0470261c21c52371f725` (Source tree `2980a614f7b0817ada98b69af0bcf5953559683e`) passed a local UE build, all 40 named Skirmish tests, all 15 named Controls tests, standalone default Victory, and the existing readability Victory at 1280x720. The readability run exercised the changed world-bounds assertion during all three camera-rig moves and produced five original captures. Native reports had zero failed tests or error entries.

A separate instrumented checkpoint `55800878ca985ca4773edaa9cac4789b47d09b40` combines this production change with the local geometry observer and its grounded parser. It passed its own build, Skirmish40, Controls15, full five-capture geometry Victory and default Victory. Actual PowerShell policy checks passed 58 cases; the parser independently transforms all eight local box corners, verifies the reported world box, preserves model X/Y and checks lower Z against model ground Z.

Matched mesh/scale/floor evidence changes the Harvester cube's lower bound from approximately -35 cm to 0 cm against the unchanged floor top at 0. Its component center is approximately 35 cm after placement. Rhino lower bounds remain 0 cm. The after run observes approximately 205 cm of same-Harvester motion and 180 cm of same-Rhino motion without model writes. Original PNGs were inspected independently. The accepted before checkpoint and its old parser remain separate; earlier failed attempts stay failed.

All runs used fresh admission samples, serial allocation and the original build/native/runtime bounds. Affinity and priority were observed and repaired by polling; this is not continuous confinement or a performance result. The 14 reused assets and preserved human-trial/rollback workspaces were unchanged.

The observer and parser are local diagnostic work, outside this seven-file production patch. Rendered evidence covers the loaded assets in these runs; signed-scale and fallback variants are synthetic/native coverage. Semantic forward direction, physical input, networking, final art and balance remain unverified. No yaw or art-style change is included.
