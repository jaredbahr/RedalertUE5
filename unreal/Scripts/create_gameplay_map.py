"""Regenerate only the tagged native gameplay fixture in its own map."""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[2]
MAP = '/Game/CommandProofGenerated/Maps/CommandProof'
TAG = 'RedalertNativeProofGenerated'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    if not level.load_level(MAP):
        raise RuntimeError('Cannot load generated gameplay map')
    for actor in actors.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            actors.destroy_actor(actor)
elif not level.new_level(MAP):
    raise RuntimeError('Cannot create gameplay map')

def spawn(kind, label, location, rotation=unreal.Rotator()):
    actor = actors.spawn_actor_from_class(kind, location, rotation)
    actor.set_actor_label(label)
    actor.set_editor_property('tags', [TAG])
    return actor

floor = spawn(unreal.StaticMeshActor, 'Native proof ground', unreal.Vector(0, 0, -15))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(unreal.Vector(60, 60, .3))
spawn(unreal.DirectionalLight, 'Native proof sun', unreal.Vector(0, 0, 1000), unreal.Rotator(-45, -35, 0))
spawn(unreal.SkyLight, 'Native proof fill', unreal.Vector(0, 0, 800))
reactor = unreal.load_asset('/Game/BlockoutGenerated/Meshes/SM_TeslaReactor_Blockout')
if not isinstance(reactor, unreal.StaticMesh):
    raise RuntimeError('Run original blockout import before native gameplay map generation')
landmark = spawn(unreal.StaticMeshActor, 'Unapproved reactor landmark', unreal.Vector(-1500, 600, 0))
landmark.static_mesh_component.set_static_mesh(reactor)
labels = sorted(actor.get_actor_label() for actor in actors.get_all_level_actors() if actor.actor_has_tag(TAG))
if len(labels) != 4:
    raise RuntimeError('Expected exactly four tagged gameplay fixture actors')
if not level.save_current_level():
    raise RuntimeError('Gameplay map save failed')
report = {'map': MAP, 'generated_actor_count': len(labels), 'generated_actor_labels': labels,
          'scope': 'Native command fixture, generic units, unapproved art; no army-scale or internet proof'}
(ROOT / 'generated' / 'native_map_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('NATIVE_MAP_OK ' + MAP)
