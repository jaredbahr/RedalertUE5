"""Reimport generated FBXs in Blender and verify units, dimensions and ground origins."""
import argparse
import json
from pathlib import Path
import sys

import bpy


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
manifest = json.loads((args.output / 'blockout_manifest.json').read_text(encoding='utf-8'))
results = []
for asset in manifest['assets']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(args.output / asset['file']))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
    assert len(meshes) == 1, (asset['file'], 'expected one combined static mesh', len(meshes))
    obj = meshes[0]
    points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
    lower = [min(point[axis] for point in points) for axis in range(3)]
    upper = [max(point[axis] for point in points) for axis in range(3)]
    dimensions = [upper[axis] - lower[axis] for axis in range(3)]
    assert all(abs(actual - expected) < .005 for actual, expected in zip(dimensions, asset['dimensions_m'])), (
        asset['file'], 'round-trip dimensions changed', dimensions, asset['dimensions_m'])
    assert abs(lower[2]) < .005, (asset['file'], 'mesh does not sit on Z=0', lower)
    assert obj.matrix_world.translation.length < .005, (asset['file'], 'origin moved')
    assert all(len(poly.vertices) >= 3 for poly in obj.data.polygons)
    results.append({'file': asset['file'], 'bounds_min_m': lower, 'bounds_max_m': upper,
                    'dimensions_m': dimensions, 'origin_m': list(obj.matrix_world.translation),
                    'mesh_count': len(meshes), 'polygons': len(obj.data.polygons), 'passed': True})
report = {'validation': 'Blender FBX round trip only; Unreal import remains unverified', 'assets': results}
(args.output / 'export_validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
