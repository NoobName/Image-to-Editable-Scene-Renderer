"""Optional, strict original-lighting contract. Existing v1 packages remain valid."""
import hashlib
from pathlib import Path
import numpy as np
try:
    from .package_schema import read_json, validate, _check_profile
    from .runtime_dds import read_dds, header
except ImportError:
    from package_schema import read_json, validate, _check_profile
    from runtime_dds import read_dds, header

SCHEMA = read_json(Path(__file__).resolve().parents[2] / 'schemas/lighting.schema.json')
_check_profile(SCHEMA)
KEYS = ('proxy', 'oldShading', 'residual', 'fitMask')
INPUTS = ('textures/original_image.png', 'analysis/normal.dds', 'analysis/albedo.dds',
          'analysis/validity.dds', 'analysis/roughness.dds', 'analysis/metallic.dds', 'analysis/region.dds', 'analysis/materialConfidence.dds')


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load_lighting(root, appearance):
    root = Path(root)
    if not (root / 'lighting/lighting.json').exists():
        return None
    try:
        from .scene_package import asset_path
    except ImportError:
        from scene_package import asset_path
    try:
        data = read_json(asset_path(root, 'lighting/lighting.json', 'lighting', ('.json',)))
        validate(data, SCHEMA, schema=SCHEMA)
        assisted = data['fit']['backend'] == 'intrinsic-assisted'
        if assisted != ('assistance' in data):
            raise ValueError('lighting assistance/backend mismatch')
        if assisted:
            info = data['assistance']; digest = info['intrinsicSha256']
            if digest:
                path = asset_path(root, 'intrinsic/intrinsic.json', 'intrinsic', ('.json',))
                if path.stat().st_size > 1024*1024 or len(digest) != 64 or sha256(path) != digest:
                    raise ValueError('lighting stale intrinsic fingerprint')
            elif info['reason'] != 'missing-intrinsic':
                raise ValueError('lighting missing intrinsic provenance')
            chosen = 'candidate' if info['selected'] else 'baseline'
            if info['selected'] != (info['reason'] == 'accepted') or data['sourceLighting'] != info[chosen+'Source']:
                raise ValueError('lighting assistance selection mismatch')
            for k,v in info[chosen+'Fit'].items():
                if data['fit'][k] != v: raise ValueError('lighting assistance fit mismatch')
            for key in ('baselineSource','candidateSource'):
                if abs(np.linalg.norm(info[key]['direction'])-1)>1e-5: raise ValueError('lighting comparison direction not unit')
        if appearance is None or data['sourceId'] != appearance['sourceId'] or data['sourceSha256'] != appearance['sourceImage']['sha256'] or data['analysisSize'] != appearance['analysisImage']['size']:
            raise ValueError('lighting/source identity or dimensions mismatch')
        w, h = data['analysisSize']; fit = data['fit']
        if fit['totalPixels'] != w*h or fit['validPixels'] > w*h or abs(fit['validFraction'] - fit['validPixels']/(w*h)) > 1e-8:
            raise ValueError('lighting fit support statistics mismatch')
        for key in ('sourceLighting', 'targetLighting'):
            if abs(np.linalg.norm(data[key]['direction']) - 1) > 1e-5:
                raise ValueError('lighting direction must be unit length')
        if [r['path'] for r in data['inputs']] != list(INPUTS):
            raise ValueError('lighting input fingerprint list mismatch')
        for record in (*data['inputs'], *data['maps'].values()):
            relative = record['path']; folder = 'lighting' if record in data['maps'].values() else relative.split('/')[0]
            path = asset_path(root, relative, folder, ('.png', '.dds'))
            if path.stat().st_size > 128*1024*1024:
                raise ValueError('lighting asset byte budget exceeded')
            digest = record['sha256']
            if len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest) or sha256(path) != digest:
                raise ValueError(f'lighting stale input/cache hash: {relative}')
        arrays = []
        for i, key in enumerate(KEYS):
            r = data['maps'][key]; fmt, array = read_dds(asset_path(root, r['path'], 'lighting', ('.dds',)))
            expected = 'RGBA32_FLOAT' if i < 2 else 'R32_FLOAT' if i == 2 else 'R32_UINT'
            if fmt != expected or r['format'] != expected or list(array.shape[:2]) != [h, w]:
                raise ValueError(f'lighting DDS dimensions/format mismatch: {key}')
            if np.any(array < 0) or i < 2 and np.any(array[..., 3] != 0) or i == 3 and np.any(array > 1):
                raise ValueError(f'lighting DDS invalid values: {key}')
            arrays.append(array)
        mask = arrays[3] > 0
        if int(mask.sum()) != fit['validPixels'] or np.any(arrays[0][~mask] != 0) or np.any(arrays[2][~mask] != 0):
            raise ValueError('lighting mask/support/invalid proxy mismatch')
        if fit['identifiable'] and (fit['albedoSource'] != 'intrinsic' or fit['status'] not in ('fitted', 'low-confidence')):
            raise ValueError('lighting identifiability/provenance mismatch')
        expected_source = 'manual-test' if fit['backend'] == 'manual-test' else 'estimated-baseline' if fit['identifiable'] else 'unreliable-baseline'
        if data['sourceLighting']['provenance'] != expected_source or (not fit['identifiable'] and fit['confidence'] > .1):
            raise ValueError('lighting source provenance/confidence mismatch')
        # Source edits in JSON must not quietly keep a preview generated from different parameters.
        _, normal = read_dds(asset_path(root, INPUTS[1], 'analysis', ('.dds',)))
        if normal.shape != (h, w, 4):
            raise ValueError('lighting normal dimensions mismatch')
        if hashlib.sha256(header(w, h, 'RGBA32_FLOAT')+normal.tobytes()).hexdigest() != data['inputs'][1]['sha256']:
            raise ValueError('lighting normal changed during load')
        source = data['sourceLighting']
        direct = np.array(source['direct']['color'])*source['direct']['intensity']
        ambient = np.array(source['ambient']['color'])*source['ambient']['intensity']
        predicted = np.maximum(normal[..., :3] @ -np.array(source['direction']), 0)[..., None]*direct+ambient
        predicted[np.linalg.norm(normal[..., :3], axis=-1) < .5] = 0
        if not np.allclose(arrays[1][..., :3], predicted, atol=1e-5, rtol=1e-5):
            raise ValueError('lighting old shading/source calibration mismatch')
        residual = np.abs((arrays[0][..., :3]-arrays[1][..., :3]) @ [.2126, .7152, .0722]); residual[~mask] = 0
        if not np.allclose(arrays[2], residual, atol=1e-5, rtol=1e-5):
            raise ValueError('lighting residual/cache mismatch')
        return data
    except (ValueError, OSError, KeyError) as error:
        raise ValueError(f'Invalid optional lighting/lighting.json: {error}') from error
