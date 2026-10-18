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

The exact combined source still needs local UE build, all 15 named Controls tests, applicable existing model gates, the repaired geometry observer's full scenario and fresh matched contact captures under the unchanged admission policy. Re-run the existing readability camera checks to validate their independent lower-bound oracle. Retain failed before attempts; do not relabel them as successful after-fix evidence.

No native compile, rendered after-fix or full geometry acceptance is claimed by the cloud implementation alone.
