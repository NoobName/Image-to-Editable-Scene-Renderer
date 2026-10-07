"""Small point-light recipe/export regression; all outputs are explicitly selected.

Prepare from a renderer-saved baseline recipe beside the new recipes. No models.
Verify after rendering the named variants into sibling point-<name> directories.
"""
import argparse
import copy
import json
from pathlib import Path
import numpy as np
from PIL import Image
from package_schema import validate, _check_profile
from verify_recipe import dds


def prepare(baseline):
    root = baseline.parent
    schema = json.loads((Path(__file__).resolve().parents[2]/'schemas/relighting-recipe.schema.json').read_text(encoding='utf-8'))
    _check_profile(schema)
    data = json.loads(baseline.read_text(encoding='utf-8'))
    data.update(version=3, rendererRevision='image-point-lights-v3', parameterContract='bounded-response-points-v3')
    data['state']['imagePointLights'] = [
        dict(id=1, position=[-.55, .25, 2.6], color=[1, .5, .12], intensity=4, range=2.4, enabled=True),
        dict(id=2, position=[.6, -.25, 2.5], color=[.15, .4, 1], intensity=3, range=2, enabled=True)]
    package = root/data['sourcePackage']
    manifest = json.loads((package/'scene.json').read_text(encoding='utf-8'))
    for name in ('edit', 'off', 'zero', 'strength-zero', 'protected'):
        value = copy.deepcopy(data)
        if name == 'off':
            for light in value['state']['imagePointLights']: light['enabled'] = False
        if name == 'zero':
            for light in value['state']['imagePointLights']: light['intensity'] = 0
        if name == 'strength-zero': value['state']['response']['strength'] = 0
        if name == 'protected':
            value['state']['protectedRegions'] = [dict(id=obj['id'], label=json.loads((package/obj['region']).read_text(encoding='utf-8'))['labelId'], weight=1) for obj in manifest['objects']]
        validate(value, schema, schema=schema)
        path = root/f'point-{name}.json'
        if path.exists():
            assert json.loads(path.read_text(encoding='utf-8')) == value, 'Existing test recipe differs'
            continue
        with path.open('x', encoding='utf-8') as stream:
            json.dump(value, stream, indent=2, ensure_ascii=False, allow_nan=False)
    # Old readers must reject, rather than silently discard, lights in a downgraded recipe.
    bad = copy.deepcopy(data)
    bad.update(version=2, rendererRevision='image-relighting-33-v2', parameterContract='bounded-response-fog-v2')
    try:
        validate(bad, schema, schema=schema)
    except ValueError:
        pass
    else:
        raise AssertionError('Downgraded point recipe accepted')
    print('Shared schema v3 / v2 rejection and point fixtures: PASS')


def verify(root):
    def buffer(name, key='result'): return dds(root/f'point-{name}'/(key+'.dds'))
    def rgb(name, key='result'): return np.asarray(Image.open(root/f'point-{name}'/(key+'.png'))).astype(int)
    report = {'nativeMaxLsb': int(np.max(np.abs(rgb('native')-rgb('native','original'))))}
    assert report['nativeMaxLsb'] <= 1
    for name in ('off', 'zero', 'strength-zero', 'protected'):
        assert np.array_equal(buffer('native'), buffer(name)), name
    for name in ('reopen', 'replayed-saved', 'ui', 'cycle', 'bad-load', 'reload', 'cancel'):
        assert np.array_equal(buffer('edit'), buffer(name)), name
    report['replayExact'] = True
    report['maxResultChange'] = float(np.max(np.abs(buffer('edit')-buffer('native'))))
    assert report['maxResultChange'] > .01
    assert np.array_equal(buffer('edit','old-shading'), buffer('native','old-shading'))
    report['newShadingChange'] = float(np.max(np.abs(buffer('edit','new-shading')-buffer('native','new-shading'))))
    assert report['newShadingChange'] > .01
    report['warpError'] = float(np.max(np.abs(buffer('edit')-buffer('warp'))))
    assert report['warpError'] < 2e-5
    # Release DXC optimizes float arithmetic; use the existing cross-backend bound.
    report['releaseError'] = float(np.max(np.abs(buffer('edit')-buffer('release'))))
    report['releaseMaxLsb'] = int(np.max(np.abs(rgb('edit')-rgb('release'))))
    assert report['releaseError'] < 2e-5 and report['releaseMaxLsb'] <= 1
    for path in root.glob('point-*/export.json'):
        data = json.loads(path.read_text(encoding='utf-8'))
        for item in data['buffers'].values():
            assert item['nonfinite'] == 0 and item['before'] == item['after'] == 128 and item['capture'] == 2048
        for numeric in path.parent.glob('*.dds'): dds(numeric)
    (root/'point-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['prepare', 'verify'])
    parser.add_argument('path', type=Path)
    args = parser.parse_args()
    (prepare if args.mode == 'prepare' else verify)(args.path)
