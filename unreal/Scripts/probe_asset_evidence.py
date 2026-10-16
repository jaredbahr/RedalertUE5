"""Finite UE editor probe; run only after a successful fresh generated import.

Adds an untagged sentinel and a duplicated engine cube outside the generated
namespace, then repeats the importer and verifies preservation and gate failures.
"""
import hashlib
import json
from pathlib import Path
import runpy
import sys

import unreal

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'art'))
from evidence import invalidate_report, validate_geometry, write_json

SENTINEL = 'RedalertEvidence_UntaggedSentinel'
EXTERNAL = '/Game/AssetEvidenceProbe/ExternalCubeSentinel'


def xyz(vector):
    return [vector.x, vector.y, vector.z]


def package_hash():
    package = ROOT / 'unreal/Content/AssetEvidenceProbe/ExternalCubeSentinel.uasset'
    if not package.is_file():
        raise RuntimeError('External sentinel package was not saved: ' + str(package))
    return hashlib.sha256(package.read_bytes()).hexdigest()


def run():
    report_path = ROOT / 'generated/ue_asset_probe.json'
    invalidate_report(report_path)
    manifest = json.loads((ROOT / 'generated/blockout_manifest.json').read_text())
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level.load_level('/Game/BlockoutGenerated/Maps/VisualProof'):
        raise RuntimeError('Fresh import VisualProof map missing')
    matches = [actor for actor in actors.get_all_level_actors()
               if actor.get_actor_label() == SENTINEL]
    if len(matches) > 1:
        raise RuntimeError('Ambiguous sentinel actors; refusing cleanup')
    if matches and not matches[0].actor_has_tag('RedalertEvidenceProbe'):
        raise RuntimeError('Sentinel label occupied by unrelated actor; refusing mutation')
    sentinel = matches[0] if matches else actors.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(1300, 700, 30))
    if not matches:
        sentinel.set_editor_property('tags', ['RedalertEvidenceProbe'])
    sentinel.set_actor_label(SENTINEL)
    if sentinel.actor_has_tag('RedalertProofGenerated'):
        raise RuntimeError('Sentinel must remain untagged by generator')
    sentinel_path = sentinel.get_path_name()
    sentinel_location = xyz(sentinel.get_actor_location())
    if not unreal.EditorAssetLibrary.does_asset_exist(EXTERNAL):
        external = unreal.EditorAssetLibrary.duplicate_asset('/Engine/BasicShapes/Cube', EXTERNAL)
        if not external:
            raise RuntimeError('Could not duplicate engine cube for external preservation probe')
        unreal.EditorAssetLibrary.set_metadata_tag(external, 'RedalertEvidenceProbe', 'v1')
        unreal.EditorAssetLibrary.save_asset(EXTERNAL, only_if_is_dirty=False)
    else:
        external = unreal.load_asset(EXTERNAL)
    if unreal.EditorAssetLibrary.get_metadata_tag(external, 'RedalertEvidenceProbe') != 'v1':
        raise RuntimeError('External probe path occupied by unrelated asset; refusing mutation')
    if not isinstance(external, unreal.StaticMesh):
        raise RuntimeError('External probe path already occupied by another asset type')
    before_hash = package_hash()
    sentinel.static_mesh_component.set_static_mesh(external)
    if not level.save_current_level():
        raise RuntimeError('Cannot save sentinel fixture')
    runpy.run_path(str(ROOT / 'unreal/Scripts/import_scene.py'), run_name='__main__')
    all_actors = actors.get_all_level_actors()
    preserved = [actor for actor in all_actors if actor.get_path_name() == sentinel_path]
    if len(preserved) != 1 or xyz(preserved[0].get_actor_location()) != sentinel_location:
        raise RuntimeError('Untagged sentinel identity/transform changed')
    if preserved[0].static_mesh_component.static_mesh.get_path_name() != external.get_path_name():
        raise RuntimeError('Sentinel mesh reference changed')
    if package_hash() != before_hash:
        raise RuntimeError('External package bytes changed across import')
    generated = [actor for actor in all_actors if actor.actor_has_tag('RedalertProofGenerated')]
    if len(generated) != 8:
        raise RuntimeError('Expected exactly eight tagged generated actors')
    record = next(asset for asset in manifest['assets'] if 'RhinoTank' in asset['file'])
    tank = next(actor for actor in generated if actor.get_actor_label() == 'SM_RhinoTank_Blockout')
    original = tank.get_actor_transform()
    placement = xyz(tank.get_actor_location())

    def check():
        center, extent = tank.get_actor_bounds(False)
        lower = [a - b for a, b in zip(xyz(center), xyz(extent))]
        upper = [a + b for a, b in zip(xyz(center), xyz(extent))]
        right, up = tank.get_actor_right_vector(), tank.get_actor_up_vector()
        validate_geometry(record, lower, upper, xyz(tank.get_actor_location()), placement=placement,
                          actor_minus_y=[-right.x, -right.y, -right.z], actor_plus_z=xyz(up))

    check()
    failures = []
    try:
        for name, scale, offset, rotation in [
            ('uniform_10x', (10, 10, 10), (0, 0, 0), 0),
            ('x_only_2x', (2, 1, 1), (0, 0, 0), 0),
            ('z_offset_100cm', (1, 1, 1), (0, 0, 100), 0),
            ('actor_facing_180', (1, 1, 1), (0, 0, 0), 180),
        ]:
            tank.set_actor_transform(original, False, True)
            tank.set_actor_scale3d(unreal.Vector(*scale))
            tank.set_actor_location(unreal.Vector(*[a + b for a, b in zip(placement, offset)]), False, True)
            tank.set_actor_rotation(unreal.Rotator(pitch=0, yaw=rotation, roll=0), True)
            applied_rotation = tank.get_actor_rotation()
            if abs(applied_rotation.pitch) > .01 or abs(applied_rotation.roll) > .01:
                raise RuntimeError('Unexpected pitch/roll in yaw-only fixture')
            if abs(abs(applied_rotation.yaw) - abs(rotation)) > .01:
                raise RuntimeError('Yaw fixture was not applied')
            try:
                check()
            except ValueError as error:
                failures.append({'case': name, 'rejected': True, 'reason': str(error),
                                 'applied_rotation_degrees': {'pitch': applied_rotation.pitch,
                                     'yaw': applied_rotation.yaw, 'roll': applied_rotation.roll}})
            else:
                raise RuntimeError('Mutation incorrectly accepted: ' + name)
    finally:
        tank.set_actor_transform(original, False, True)
    check()
    if not level.save_current_level():
        raise RuntimeError('Cannot save restored probe map')
    report = {'run_id': manifest['run_id'], 'source_sha256': manifest['source_sha256'],
              'passed': True, 'status': 'passed', 'sentinel_actor': sentinel_path,
              'external_asset': EXTERNAL, 'external_package_sha256_before_and_after': before_hash,
              'generated_actor_count': len(generated), 'actual_actor_mutations': failures,
              'material_slots': {actor.get_actor_label(): [str(slot.material_slot_name)
                  for slot in actor.static_mesh_component.static_mesh.static_materials]
                  for actor in generated if actor.get_actor_label().startswith('SM_')},
              'unverified': ['material color-update reimport', 'generic material name collision',
                             'mutated FBX mesh-local pivot/vertex offset', 'visual semantic front']}
    write_json(report_path, report)
    unreal.log('REDALERT_ASSET_PROBE_OK ' + str(report_path))


run()
