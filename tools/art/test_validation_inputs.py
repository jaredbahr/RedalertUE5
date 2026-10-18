"""Fail-closed geometry and real export-entrypoint regressions; no bpy required."""
import contextlib
from copy import deepcopy
import io
import itertools
import json
from pathlib import Path
import runpy
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from evidence import sha256, source_hash, validate_geometry, write_json


ASSET = {'dimensions_m': [3, 6, 2], 'bounds_min_m': [-1.5, -4, 0],
         'bounds_max_m': [1.5, 2, 2]}


class GeometryInputs(unittest.TestCase):
    def check(self, asset=None, **kwargs):
        values = {'lower': [-150, -200, 0], 'upper': [150, 400, 200],
                  'pivot': [0, 0, 0]}
        values.update(kwargs)
        validate_geometry(ASSET if asset is None else asset, **values)

    def test_valid_controls(self):
        self.check()
        self.check(lower=(-150, -200, 0), upper=(150, 400, 200), pivot=(0, 0, 0))
        self.check(lower=[600, -200, 0], upper=[900, 400, 200],
                   pivot=[750, 0, 0], placement=[750, 0, 0])
        self.check(lower=[-1.5, -4, 0], upper=[1.5, 2, 2], factor=1, flip_y=False)

    def test_expected_geometry_rejects_nonfinite_and_nonnumeric(self):
        for key, axis, value in itertools.product(ASSET, range(3),
                [float('nan'), float('inf'), -float('inf'), None, '0', True]):
            asset = deepcopy(ASSET)
            asset[key][axis] = value
            with self.subTest(key=key, axis=axis, value=value), self.assertRaises(ValueError):
                self.check(asset)

    def test_required_manifest_vectors(self):
        for key in ASSET:
            for value in (None, [], [0], [0, 0], [0, 0, 0, 0], '000', {0: 0, 1: 0, 2: 0}):
                asset = deepcopy(ASSET)
                asset[key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    self.check(asset)
            asset = deepcopy(ASSET)
            del asset[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                self.check(asset)

    def test_actual_vectors_require_three_finite_numbers(self):
        for key in ('lower', 'upper', 'pivot', 'placement', 'actor_minus_y', 'actor_plus_z'):
            for value in (None, [], [0], [0, 0], [0, 0, 0, 0], '000',
                          [float('nan'), 0, 0], [0, float('inf'), 0], [0, 0, -float('inf')],
                          [True, 0, 0], ['0', 0, 0]):
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    self.check(**{key: value})

    def test_invalid_scale_and_overflow(self):
        for factor in (0, -1, float('nan'), float('inf'), None, True, '100', 1e308):
            with self.subTest(factor=factor), self.assertRaises(ValueError):
                self.check(factor=factor)

    def test_negative_dimensions_and_reversed_bounds(self):
        for key, value in (('dimensions_m', [-3, 6, 2]), ('bounds_min_m', [2, -4, 0])):
            asset = deepcopy(ASSET)
            asset[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.check(asset)
        with self.assertRaises(ValueError):
            self.check(lower=[150, -200, 0], upper=[-150, 400, 200])


class ExportEntrypoint(unittest.TestCase):
    def run_export(self, output, bpy):
        script = Path(__file__).with_name('validate_exports.py')
        with patch.object(sys, 'argv', [str(script), '--', '--output', str(output)]), \
                patch.dict(sys.modules, {'bpy': bpy}), contextlib.redirect_stdout(io.StringIO()):
            runpy.run_path(str(script), run_name='__main__')

    def manifest(self, assets):
        return {'run_id': 'test-run', 'source_sha256': source_hash(Path(__file__).resolve().parents[2]),
                'generation_status': 'complete', 'assets': assets}

    def test_empty_and_invalid_assets_cannot_preserve_success(self):
        for assets in ([], None, {}, ''):
            with self.subTest(assets=assets), tempfile.TemporaryDirectory() as temp:
                output = Path(temp)
                write_json(output / 'blockout_manifest.json', self.manifest(assets))
                write_json(output / 'export_validation.json', {'passed': True})
                bpy = SimpleNamespace(ops=Mock())
                with self.assertRaisesRegex(ValueError, 'non-empty assets list'):
                    self.run_export(output, bpy)
                report = json.loads((output / 'export_validation.json').read_text())
                self.assertIs(report['passed'], False)
                self.assertEqual(report['status'], 'checking_or_failed')
                bpy.ops.assert_not_called()
                self.assertEqual(bpy.ops.mock_calls, [])

    def test_valid_nonempty_export_passes_real_entrypoint(self):
        # Stub only the Blender boundary; actual provenance/geometry gates run.
        class Matrix:
            translation = [0, 0, 0]

            def __matmul__(self, value):
                return value

        points = list(itertools.product((-1.5, 1.5), (-4, 2), (0, 2)))
        obj = SimpleNamespace(type='MESH', matrix_world=Matrix(), data=SimpleNamespace(
            vertices=[SimpleNamespace(co=point) for point in points],
            polygons=[SimpleNamespace(vertices=[0, 1, 2])]))
        bpy = SimpleNamespace(ops=Mock(), context=SimpleNamespace(scene=SimpleNamespace(objects=[obj])))
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)
            asset_file = output / 'test.fbx'
            asset_file.write_bytes(b'test geometry')
            asset = {**deepcopy(ASSET), 'file': asset_file.name, 'sha256': sha256(asset_file)}
            write_json(output / 'blockout_manifest.json', self.manifest([asset]))
            self.run_export(output, bpy)
            report = json.loads((output / 'export_validation.json').read_text())
            self.assertIs(report['passed'], True)
            self.assertEqual(len(report['assets']), 1)
            self.assertEqual(report['assets'][0]['dimensions_m'], [3, 6, 2])
            bpy.ops.import_scene.fbx.assert_called_once_with(filepath=str(asset_file))


if __name__ == '__main__':
    unittest.main()
