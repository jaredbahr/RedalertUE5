"""Small dependency-free provenance and geometry gates shared by Blender and UE."""
import hashlib
import json
import math
from pathlib import Path
import shutil
import uuid


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_hash(root):
    root = Path(root)
    paths = sorted(list((root / 'tools' / 'art').glob('*.py')) +
                   list((root / 'unreal' / 'Scripts').glob('*.py')))
    digest = hashlib.sha256()
    for path in paths:
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2), encoding='utf-8')


def invalidate_report(path, identity=None):
    write_json(path, {**(identity or {}), 'status': 'checking_or_failed', 'passed': False})


def begin_run(output, root):
    """Archive only pipeline-owned evidence. Never touch user baseline folders."""
    output = Path(output)
    run_id = uuid.uuid4().hex
    archive = output / 'evidence_history' / run_id
    owned = ['previews', 'blockout_manifest.json', 'export_validation.json', 'ue_import_report.json', 'ue_asset_probe.json', 'ue_negative_probe.json']
    for name in owned:
        source = output / name
        if source.exists():
            archive.mkdir(parents=True, exist_ok=True)
            shutil.move(str(source), str(archive / name))
    if archive.exists():
        write_json(archive / 'ARCHIVED.json', {'status': 'historical, not current evidence',
                                              'superseded_by_run': run_id})
    identity = {'run_id': run_id, 'source_sha256': source_hash(root)}
    for name in ('export_validation.json', 'ue_import_report.json', 'ue_asset_probe.json', 'ue_negative_probe.json'):
        write_json(output / name, {**identity, 'status': 'not_run', 'passed': False})
    return identity


def verify_inputs(output, manifest, root):
    if manifest['source_sha256'] != source_hash(root):
        raise ValueError('Generator/validator source changed; regenerate before validating')
    for asset in manifest['assets']:
        if sha256(Path(output) / asset['file']) != asset['sha256']:
            raise ValueError('Export hash changed: ' + asset['file'])


def validate_geometry(asset, lower, upper, pivot, placement=(0, 0, 0),
                      actor_minus_y=(0, -1, 0), actor_plus_z=(0, 0, 1), factor=100, flip_y=True):
    """Validate actual world bounds and basis; tolerances are 0.5cm or 0.1%."""
    def close(actual, expected, tolerance, label):
        if not math.isfinite(actual) or abs(actual - expected) > tolerance:
            raise ValueError(f'{label}: actual {actual}, expected {expected}, tolerance {tolerance}')
    for axis in range(3):
        expected = asset['dimensions_m'][axis] * factor
        close(upper[axis] - lower[axis], expected, max(.005 * factor, expected * .001),
              'XYZ'[axis] + ' dimension')
        close(pivot[axis], placement[axis], .005 * factor, 'pivot ' + 'XYZ'[axis])
        low = asset['bounds_min_m'][axis]
        high = asset['bounds_max_m'][axis]
        if axis == 1 and flip_y:
            low, high = -high, -low
        close(lower[axis] - placement[axis], low * factor, .005 * factor, 'local lower ' + 'XYZ'[axis])
        close(upper[axis] - placement[axis], high * factor, .005 * factor, 'local upper ' + 'XYZ'[axis])
    close(lower[2], placement[2], .005 * factor, 'ground contact')
    for actual, expected in zip(actor_minus_y, (0, -1, 0)):
        close(actual, expected, .001, 'actor local -Y forward')
    for actual, expected in zip(actor_plus_z, (0, 0, 1)):
        close(actual, expected, .001, 'actor local +Z up')


def require_export_validation(output, manifest):
    report = json.loads((Path(output) / 'export_validation.json').read_text(encoding='utf-8'))
    expected_hashes = {asset['file']: asset['sha256'] for asset in manifest['assets']}
    if (report.get('passed') is not True or report.get('run_id') != manifest['run_id'] or
            report.get('source_sha256') != manifest['source_sha256'] or
            report.get('asset_hashes') != expected_hashes):
        raise ValueError('Current FBX round-trip validation is required before UE import')
