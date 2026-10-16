# Bounded material and real-FBX negative probes

This lane prepares executable acceptance checks; root schedules heavy tools. Blockout design and final palette stay unchanged. The code has no new dependencies.

1. Generate baseline geometry with the current scripts, then run the normal Blender FBX validator. The manifest now carries the authored palette. Import requires a successful matching run/source/asset-hash FBX round-trip report; malformed/corrupt geometry is rejected before mutating UE content.
2. Run `tools/art/prepare_negative_fixtures.py` in Blender against that baseline. It retains three ignored, uniquely named fixture sets and a current index: changed Red palette, tank vertices translated +1 m Z at fixed pivot, and tank pivot translated +1 m Z while counter-translating vertices to preserve world geometry. Every fixture is actually reimported in Blender against baseline bounds/pivot, with rejection reasons recorded. Corrupted files have matching new byte hashes; they cannot be dismissed merely as hash mismatches. An incomplete fixture run invalidates its index.
3. Root executes `unreal/Scripts/probe_negative_assets.py` using the same UE commandlet/cache environment as the normal importer. The probe creates a metadata-owned unrelated generic material `/Game/AssetEvidenceProbe/GenericCollision/Red` and checks its saved package SHA-256 across baseline, changed-color, negative attempts and restoration. Collisions with unrelated content at the exact test path abort.
4. Baseline/changed material acceptance reads the actual Red mesh slots on all three imported meshes, verifies their generated material-instance paths and `AuthoredColor` values, and requires the changed values. The importer explicitly builds/refreshes generated palette materials from manifest colors rather than relying on legacy FBX `replace_existing` to refresh material graphs. FBX material search is local to the reserved generated import destination. This is an explicit palette contract, not a claim of automatic FBX shader parity.
5. Each actual corrupt-FBX fixture must leave `ue_import_report.json` false by failing the required round-trip acceptance gate. The report names that gate: **this does not claim UE independently rejected corrupt pivot metadata or prove whether its coordinate conversion would correct it**. No unsafe validation bypass is added. Blender's signed bounds/pivot checks reject the actual reimported bytes.
6. A `finally` block restores exact original manifest, export-validation report and FBX bytes from a uniquely named ignored input archive, runs the normal UE import, and rechecks original Red colors. The final negative-probe report is successful only after clean restoration and unchanged external package bytes. No content deletion or source-art polishing occurs. Generated actor preservation is covered separately by the existing probe.

Scheduled commands, from this worktree (not executed by the authoring agent):

```powershell
$blender = 'C:\Program Files\Blender Foundation\Blender 4.5\blender.exe'
$output = Join-Path (Get-Location) 'generated'
& $blender --background --factory-startup --threads 4 --python tools/art/build_blockout.py -- --output $output
& $blender --background --factory-startup --threads 4 --python tools/art/validate_exports.py -- --output $output
& $blender --background --factory-startup --threads 4 --python tools/art/prepare_negative_fixtures.py -- --output $output
# Root supplies existing process-local D: cache/temp, four-core limit and NullRHI commandlet options:
# -run=pythonscript -script=<absolute unreal/Scripts/probe_negative_assets.py>
```

Cheap validation: four tests pass with `python -m unittest discover -s tools/art -p test_evidence.py -v`, including export-report success/run/source/hash mismatches, actual-entrypoint malformed-input invalidation and fixture-index preservation. No rendered art approval, visual semantic-facing result, network result or scale result follows from these tests.

## Observed root execution

The reusable runner is `unreal/Scripts/test-asset-negative.ps1`. It records exact HEAD/dirty state, UTC timings, per-stage process outcomes, source/run identity, all three cases and verified restoration under ignored `generated/negative-run/`.

| Evidence | Observed result |
| --- | --- |
| Tested source | `d99a6931d1a194986c7c22d68074d1197d8dd62d`, clean working tree |
| Time | 2026-10-16 04:52:39.670 UTC to 04:53:57.861 UTC; 78.190 seconds total |
| Run | `1a93a1f6a31a4b13b899d85bb77d0b4d` |
| Palette | Actual Red slots on all three UE meshes changed from approximately `(0.53,0.035,0.025,1)` to `(0.025,0.55,0.8,1)` through the explicit generated material contract. |
| Vertex fixture | Reimported tank vertices had lower Z=1 m rather than 0; Blender gate rejected them. |
| Pivot fixture | Reimported tank pivot had Z=1 m rather than 0 while world geometry stayed in place; Blender gate rejected it. |
| UE negative attempts | Both refused the failed matching FBX round-trip report and left import success false, before UE mutation. |
| Restoration | Original manifest, export report, all three FBX byte sequences and baseline material colors restored; unrelated generic material package hash unchanged. |

The retained runner result is `generated/negative-run/20261016T045239670Z-b1f6f947608d4cdbbbab44c9cf74d093/result.json`; detailed observations are in `generated/ue_negative_probe.json`. Source hash is `541baf195e89f8c49e92e79452caa7f02d9237f9f28eeae015f69319a2fc0f01`.

An earlier clean run at `e1c61c2` passed Blender stages but failed before UE imports because `material_search_location` was set on `FbxImportUI`. Installed engine headers place it on `texture_import_data`; the corrected call was committed before the successful full rerun. That earlier run is not acceptance evidence. A timeout or killed process can interrupt restoration; failed runs must be inspected before reuse.

Independent evidence review verified current source/FBX hashes, exact byte restoration against the retained input archive, the external package hash and all three cases. A subsequent PowerShell-only correction removes a redundant parameterless `WaitForExit()` after `HasExited`, preventing redirect drain from escaping the deadline. Syntax/review checks cover that correction; the measured full run above remains explicitly attributed to `d99a693`, with unchanged Python source and acceptance gates.
