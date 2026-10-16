"""Bounded palette/collision/real-FBX negative acceptance probe; root schedules UE."""
import json
from pathlib import Path
import runpy
import shutil
import sys
import uuid

import unreal

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/art'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from evidence import invalidate_report, sha256, verify_inputs, write_json
from palette_materials import PARAMETER

OUTPUT = ROOT / 'generated'
REPORT = OUTPUT / 'ue_negative_probe.json'
EXTERNAL = '/Game/AssetEvidenceProbe/GenericCollision/Red'


def run_import():
    runpy.run_path(str(ROOT / 'unreal/Scripts/import_scene.py'), run_name='__main__')


def stage(directory):
    for name in ('blockout_manifest.json', 'export_validation.json'):
        shutil.copyfile(directory / name, OUTPUT / name)
    manifest = json.loads((directory / 'blockout_manifest.json').read_text())
    for asset in manifest['assets']:
        shutil.copyfile(directory / asset['file'], OUTPUT / asset['file'])
    return manifest


def color_evidence(expected):
    matched = []
    for name in ('TeslaReactor', 'RhinoTank', 'Conscript'):
        mesh = unreal.load_asset('/Game/BlockoutGenerated/Meshes/SM_' + name + '_Blockout')
        for slot in mesh.static_materials:
            if str(slot.material_slot_name) == 'Red':
                material = slot.material_interface
                if not material.get_path_name().startswith('/Game/BlockoutGenerated/Materials/'):
                    raise RuntimeError('Red slot references material outside generated palette')
                color = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material, PARAMETER)
                rgba = [color.r, color.g, color.b, color.a]
                if any(abs(a - b) > .0001 for a, b in zip(rgba, expected)):
                    raise RuntimeError('Actual mesh slot color mismatch: ' + str(rgba))
                matched.append({'mesh': mesh.get_path_name(), 'material': material.get_path_name(), 'rgba': rgba})
    if len(matched) != 3:
        raise RuntimeError('Expected a Red material slot on every proof mesh')
    return matched


def run():
    invalidate_report(REPORT)
    baseline = json.loads((OUTPUT / 'blockout_manifest.json').read_text())
    verify_inputs(OUTPUT, baseline, ROOT)
    index = json.loads((OUTPUT / 'asset_probe_fixtures/index.json').read_text())
    if index.get('passed') is not True:
        raise RuntimeError('Fixture generation did not complete')
    if index['baseline_run_id'] != baseline['run_id'] or index['source_sha256'] != baseline['source_sha256']:
        raise RuntimeError('Fixture index does not match clean baseline/source')
    archive = OUTPUT / 'negative_probe_history' / uuid.uuid4().hex
    archive.mkdir(parents=True)
    (archive / 'assets').mkdir()
    # Snapshot only exact generated inputs; old captures/baselines are untouched.
    for name in ('blockout_manifest.json', 'export_validation.json'):
        shutil.copyfile(OUTPUT / name, archive / name)
    for asset in baseline['assets']:
        shutil.copyfile(OUTPUT / asset['file'], archive / asset['file'])
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    external = unreal.load_asset(EXTERNAL)
    if external is None:
        external = tools.create_asset('Red', '/Game/AssetEvidenceProbe/GenericCollision',
                                     unreal.Material, unreal.MaterialFactoryNew())
        unreal.EditorAssetLibrary.set_metadata_tag(external, 'RedalertNegativeProbe', 'v1')
        unreal.EditorAssetLibrary.save_asset(EXTERNAL)
    if (not isinstance(external, unreal.Material) or
            unreal.EditorAssetLibrary.get_metadata_tag(external, 'RedalertNegativeProbe') != 'v1'):
        raise RuntimeError('Generic collision path occupied by unrelated content; refusing mutation')
    external_package = ROOT / 'unreal/Content/AssetEvidenceProbe/GenericCollision/Red.uasset'
    before_hash = sha256(external_package)
    observations = []
    restored = False
    try:
        run_import()
        baseline_colors = color_evidence(baseline['palette']['Red'])
        for fixture in index['cases']:
            manifest = stage(Path(fixture['directory']))
            if fixture['case'] == 'palette_change':
                run_import()
                slots = color_evidence(manifest['palette']['Red'])
                observations.append({'case': fixture['case'], 'passed': True,
                                     'baseline_slots': baseline_colors, 'changed_slots': slots,
                                     'classification': 'explicit manifest palette refresh applied to actual UE mesh slots'})
            else:
                try:
                    run_import()
                except ValueError as error:
                    report = json.loads((OUTPUT / 'ue_import_report.json').read_text())
                    if report.get('passed') is not False or 'FBX round-trip validation' not in str(error):
                        raise RuntimeError('Unexpected negative rejection path: ' + str(error))
                    observations.append({'case': fixture['case'], 'passed': True,
                        'classification': 'rejected at required real-FBX round-trip acceptance gate before UE mutation',
                        'reason': str(error), 'blender_errors': fixture['errors'],
                        'ue_import_report_passed': report['passed']})
                else:
                    raise RuntimeError('Corrupt FBX fixture was accepted: ' + fixture['case'])
            if sha256(external_package) != before_hash:
                raise RuntimeError('Unrelated generic Red material package changed')
    finally:
        stage(archive)
        run_import()
        color_evidence(baseline['palette']['Red'])
        restored = True
    if sha256(external_package) != before_hash:
        raise RuntimeError('External material changed during restoration')
    write_json(REPORT, {'run_id': baseline['run_id'], 'source_sha256': baseline['source_sha256'],
        'status': 'passed', 'passed': True, 'cases': observations, 'restored_original_inputs_and_colors': restored,
        'external_generic_material': EXTERNAL, 'external_package_sha256_before_and_after': before_hash,
        'retained_input_archive': str(archive),
        'limits': ['No direct UE importer bypass of failed FBX gate',
                   'No claim FBX pivot corruption survives or is corrected by UE conversion',
                   'No final art approval or rendered visual semantic-front proof']})
    unreal.log('REDALERT_NEGATIVE_PROBE_OK ' + str(REPORT))


run()
