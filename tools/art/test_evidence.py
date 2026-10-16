"""Independent numerical fixtures and stale-evidence regression; no bpy required."""
import json
from pathlib import Path
import tempfile
import runpy
import sys
from types import SimpleNamespace
from unittest.mock import patch
import unittest
from evidence import begin_run, invalidate_report, require_export_validation, sha256, validate_geometry, verify_inputs, write_json


class Gates(unittest.TestCase):
    def test_geometry_mutations(self):
        # Asymmetric authored fixture: 3m wide, 6m long, 2m tall; pivot at ground.
        asset = {'dimensions_m': [3, 6, 2], 'bounds_min_m': [-1.5, -4, 0],
                 'bounds_max_m': [1.5, 2, 2]}
        lower, upper = [-150, -200, 0], [150, 400, 200]
        validate_geometry(asset, lower, upper, [0, 0, 0])
        for label, lo, hi, pivot, actor_minus_y in [
            ('10x scale', [-1500, -2000, 0], [1500, 4000, 2000], [0, 0, 0], [0, -1, 0]),
            ('X scale', [-300, -200, 0], [300, 400, 200], [0, 0, 0], [0, -1, 0]),
            ('mesh Z offset', [-150, -200, 100], [150, 400, 300], [0, 0, 0], [0, -1, 0]),
            ('pivot Z offset', lower, upper, [0, 0, 100], [0, -1, 0]),
            ('180 facing', [-150, -400, 0], [150, 200, 200], [0, 0, 0], [0, 1, 0]),
            ('basis only', lower, upper, [0, 0, 0], [0, 1, 0]),
        ]:
            with self.subTest(label=label), self.assertRaises(ValueError):
                validate_geometry(asset, lo, hi, pivot, actor_minus_y=actor_minus_y)

    def test_real_entrypoints_invalidate_before_bad_manifest(self):
        # Execute actual script entrypoints with inert engine modules. Malformed input
        # must fail before any bpy/unreal API is needed, clearing previous successes.
        source_root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory(dir=source_root) as temp:
            root = Path(temp)
            output = root / 'generated'
            output.mkdir()
            (output / 'blockout_manifest.json').write_text('{bad json')
            for relative, reports in [
                ('tools/art/validate_exports.py', ['export_validation.json']),
                ('tools/art/prepare_negative_fixtures.py', ['asset_probe_fixtures/index.json']),
                ('unreal/Scripts/import_scene.py', ['ue_import_report.json', 'ue_asset_probe.json']),
            ]:
                script = root / relative
                script.parent.mkdir(parents=True, exist_ok=True)
                script.write_text((source_root / relative).read_text())
                if relative.startswith('unreal/'):
                    (script.parent / 'palette_materials.py').write_text(
                        (source_root / 'unreal/Scripts/palette_materials.py').read_text())
                for name in reports:
                    (output / name).parent.mkdir(parents=True, exist_ok=True)
                    write_json(output / name, {'passed': True, 'run_id': 'same-run'})
                argv = [str(script), '--', '--output', str(output)]
                with self.subTest(script=relative), patch.object(sys, 'argv', argv), \
                        patch.dict(sys.modules, {'bpy': SimpleNamespace(), 'unreal': SimpleNamespace(),
                                                 'mathutils': SimpleNamespace(Vector=object)}):
                    with self.assertRaises(json.JSONDecodeError):
                        runpy.run_path(str(script), run_name='__main__')
                for name in reports:
                    self.assertFalse(json.loads((output / name).read_text())['passed'])

    def test_export_acceptance_requires_matching_success(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)
            manifest = {'run_id': 'new', 'source_sha256': 'source',
                        'assets': [{'file': 'tank.fbx', 'sha256': 'bytes'}]}
            valid = {'run_id': 'new', 'source_sha256': 'source', 'passed': True,
                     'asset_hashes': {'tank.fbx': 'bytes'}}
            write_json(output / 'export_validation.json', valid)
            require_export_validation(output, manifest)
            for changed in [{'passed': False}, {'run_id': 'old'},
                            {'source_sha256': 'old'}, {'asset_hashes': {'tank.fbx': 'other'}}]:
                write_json(output / 'export_validation.json', {**valid, **changed})
                with self.subTest(changed=changed), self.assertRaises(ValueError):
                    require_export_validation(output, manifest)

    def test_success_change_geometry_only_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'tools/art').mkdir(parents=True)
            script = root / 'tools/art/generator.py'
            script.write_text('original source')
            output = root / 'generated'
            output.mkdir()
            (output / 'assets').mkdir()
            asset = output / 'assets/test.fbx'
            asset.write_bytes(b'original geometry')
            identity = begin_run(output, root)
            manifest = {**identity, 'assets': [{'file': 'assets/test.fbx', 'sha256': sha256(asset)}]}
            write_json(output / 'blockout_manifest.json', manifest)
            verify_inputs(output, manifest, root)
            write_json(output / 'export_validation.json', {**identity, 'passed': True})
            write_json(output / 'ue_asset_probe.json', {**identity, 'passed': True})
            write_json(output / 'asset_probe_fixtures/index.json', {**identity, 'passed': True})
            fixture_binary = output / 'asset_probe_fixtures/prior_geometry.fbx'
            fixture_binary.write_bytes(b'preserved fixture')
            (output / 'previews').mkdir()
            (output / 'previews/approved.png').write_bytes(b'baseline capture')
            script.write_text('changed source')
            second = begin_run(output, root)
            self.assertFalse((output / 'previews').exists())
            self.assertFalse(json.loads((output / 'ue_asset_probe.json').read_text())['passed'])
            self.assertEqual(json.loads((output / 'ue_asset_probe.json').read_text())['run_id'], second['run_id'])
            archive = output / 'evidence_history' / second['run_id']
            self.assertFalse(json.loads((output / 'asset_probe_fixtures/index.json').read_text())['passed'])
            self.assertTrue(json.loads((archive / 'asset_probe_fixtures/index.json').read_text())['passed'])
            self.assertEqual(fixture_binary.read_bytes(), b'preserved fixture')
            self.assertEqual((archive / 'previews/approved.png').read_bytes(), b'baseline capture')
            asset.write_bytes(b'changed geometry')
            current = {**second, 'previews': {'status': 'omitted'},
                       'assets': [{'file': 'assets/test.fbx', 'sha256': sha256(asset)}]}
            write_json(output / 'blockout_manifest.json', current)
            verify_inputs(output, current, root)
            # A same-run revalidation must also invalidate a previous current pass.
            write_json(output / 'export_validation.json', {**second, 'passed': True})
            asset.write_bytes(b'forced bad export')
            invalidate_report(output / 'export_validation.json', second)
            with self.assertRaises(ValueError):
                verify_inputs(output, current, root)
            self.assertFalse(json.loads((output / 'export_validation.json').read_text())['passed'])
            self.assertTrue(json.loads((archive / 'export_validation.json').read_text())['passed'])
            self.assertTrue(json.loads((archive / 'ue_asset_probe.json').read_text())['passed'])


if __name__ == '__main__':
    unittest.main()
