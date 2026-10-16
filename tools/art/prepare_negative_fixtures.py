"""Prepare finite FBX probes from a clean generated baseline; no source art changes."""
import argparse
import copy
import json
from pathlib import Path
import sys
import uuid

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from evidence import sha256, verify_inputs, write_json, validate_geometry

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
output = args.output.resolve()
if output.drive.upper() != 'D:':
    raise ValueError('Probe output must remain on D:')
manifest = json.loads((output / 'blockout_manifest.json').read_text())
verify_inputs(output, manifest, Path(__file__).resolve().parents[2])
fixtures = output / 'asset_probe_fixtures'
fixtures.mkdir(exist_ok=True)
index_path = fixtures / 'index.json'
if index_path.exists():
    (fixtures / ('historical_index_' + uuid.uuid4().hex + '.json')).write_bytes(index_path.read_bytes())
write_json(index_path, {'passed': False, 'status': 'checking_or_failed'})
summary = []
for case in ('palette_change', 'vertex_z_offset', 'pivot_z_offset'):
    directory = fixtures / (case + '_' + uuid.uuid4().hex)
    directory.mkdir()
    (directory / 'assets').mkdir()
    modified = copy.deepcopy(manifest)
    modified['run_id'] = uuid.uuid4().hex
    modified['fixture'] = case
    if case == 'palette_change':
        modified['palette']['Red'] = [.025, .55, .8, 1.0]
    for asset in modified['assets']:
        source = output / asset['file']
        destination = directory / asset['file']
        # Only the tank needs geometry corruption; every mesh shares palette contract.
        mutate = case == 'palette_change' or 'RhinoTank' in asset['file']
        if not mutate:
            destination.write_bytes(source.read_bytes())
            continue
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.scene.unit_settings.system = 'METRIC'
        bpy.context.scene.unit_settings.scale_length = 1.0
        bpy.ops.import_scene.fbx(filepath=str(source))
        obj = next(obj for obj in bpy.context.scene.objects if obj.type == 'MESH')
        if case == 'palette_change':
            for material in obj.data.materials:
                if material.name == 'Red':
                    material.diffuse_color = tuple(modified['palette']['Red'])
                    if material.use_nodes:
                        for node in material.node_tree.nodes:
                            if node.type == 'BSDF_PRINCIPLED':
                                node.inputs['Base Color'].default_value = tuple(modified['palette']['Red'])
        elif case == 'vertex_z_offset':
            delta = obj.matrix_world.inverted().to_3x3() @ Vector((0, 0, 1))
            for vertex in obj.data.vertices:
                vertex.co += delta
        else:
            # Shift actual local pivot while keeping world-space geometry in place.
            delta = obj.matrix_world.inverted().to_3x3() @ Vector((0, 0, 1))
            for vertex in obj.data.vertices:
                vertex.co -= delta
            obj.location.z += 1
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.fbx(filepath=str(destination), use_selection=True, object_types={'MESH'},
                                 axis_forward='-Y', axis_up='Z', apply_unit_scale=True,
                                 bake_anim=False, add_leaf_bones=False)
    for asset in modified['assets']:
        asset['sha256'] = sha256(directory / asset['file'])
    write_json(directory / 'blockout_manifest.json', modified)
    # Perform a true FBX reimport on fixture bytes, preserving original geometry contract.
    errors = []
    for asset in modified['assets']:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(directory / asset['file']))
        obj = next(obj for obj in bpy.context.scene.objects if obj.type == 'MESH')
        points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
        lower = [min(point[axis] for point in points) for axis in range(3)]
        upper = [max(point[axis] for point in points) for axis in range(3)]
        try:
            validate_geometry(asset, lower, upper, list(obj.matrix_world.translation), factor=1, flip_y=False)
        except ValueError as error:
            errors.append({'asset': asset['file'], 'reason': str(error)})
    expected_pass = case == 'palette_change'
    if (not errors) != expected_pass:
        raise RuntimeError('Unexpected fixture round-trip classification: ' + case + ': ' + str(errors))
    write_json(directory / 'export_validation.json', {
        'run_id': modified['run_id'], 'source_sha256': modified['source_sha256'],
        'passed': not errors, 'status': 'passed' if not errors else 'failed',
        'asset_hashes': {asset['file']: asset['sha256'] for asset in modified['assets']}, 'errors': errors})
    summary.append({'case': case, 'directory': str(directory), 'round_trip_passed': not errors, 'errors': errors})
write_json(fixtures / 'index.json', {'passed': True, 'baseline_run_id': manifest['run_id'],
                                    'source_sha256': manifest['source_sha256'], 'cases': summary})
print('REDALERT_FBX_FIXTURES_READY ' + str(fixtures / 'index.json'))
