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

Cheap validation: `python -m unittest discover -s tools/art -p test_evidence.py -v` includes export-report success/run/source/hash mismatches and actual-entrypoint malformed-input invalidation. Engine/Blender execution remains pending; API presence, material-slot naming and fixture behavior must be measured before publishing a pass claim. No rendered art approval, visual semantic-facing result, network result or scale result follows from these tests.
