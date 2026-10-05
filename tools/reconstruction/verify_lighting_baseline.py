"""Check actual Prompt19 windows, GPU readbacks, unchanged pixels and offline fitting evidence."""
import json
import re
from pathlib import Path
import numpy as np
from PIL import Image
from runtime_dds import read_dds
from scene_package import write_json_atomic
from pipeline.material_estimation_backend import linear_to_srgb, quantize


def verify():
    root = Path('generated'); report = {'windows': {}, 'pixelChecks': {}, 'preservedFiles': []}
    def read(path):
        value = json.loads(Path(path).read_text(encoding='utf-8')); json.dumps(value, allow_nan=False); return value
    def info(name): return read(root/f'prompt19-{name}.view.json')
    def pixels(path): return np.asarray(Image.open(path).convert('RGB'), dtype=np.int16)
    def compare(name, a, b, tolerance=0):
        aa, bb = pixels(a), pixels(b); assert aa.shape == bb.shape, name
        error = np.abs(aa-bb); report['pixelChecks'][name] = {'maxLSB': int(error.max()), 'changedPixels': int(np.any(error, -1).sum())}
        assert error.max() <= tolerance, (name, report['pixelChecks'][name])
    def capture(name): return root/f'prompt19-{name}.bmp'
    compare('3D Final retained', capture('before-final'), capture('after-final'))
    compare('Source retained', capture('before-source'), capture('after-source'))
    compare('Canonical native', capture('real-original'), root/'scene19-final/textures/source_anchor.png', 1)
    for name in ('target', 'transaction', 'cycle', 'failed', 'cancelled', 'release', 'warp-views', 'broken-load'):
        # UNORM sRGB display arithmetic may round at adjacent LSBs across adapters; raw floats must still match exactly.
        compare(name, capture(name), capture('first-window'), 1 if name == 'warp-views' else 0)
    assert info('source-invalidated')['lighting']['cacheValid'] is False
    assert info('target')['lighting']['cacheValid'] is True
    for name in ('target', 'transaction', 'cycle', 'failed', 'cancelled'):
        for key in ('sourceId', 'sourceSha256', 'sourceCamera', 'analysisMapping', 'revision'):
            assert info(name)[key] == info('first-window')[key], (name, key)
    names = ['real-'+v for v in ('original', 'shading-proxy', 'old-shading', 'lighting-residual', 'fit-mask')]
    names += ['after-final', 'after-source', 'target', 'source-invalidated', 'transaction', 'warp-views', 'cycle', 'ui', 'wide', 'narrow', 'release', 'unavailable']
    names += ['synthetic-'+v for v in ('lambert', 'plane', 'black', 'overexposed', 'low-validity', 'neutral', 'warp')]
    names += ['reload', 'failed', 'cancelled', 'reconstruct', 'broken-load', 'manual-window', 'final-warp']
    for name in names:
        log = (root/f'prompt19-{name}.log').read_text(encoding='utf-8'); view = info(name)
        assert 'Window created' in log and 'Completed frames=' in log
        assert ('Validation summary: disabled' if name == 'release' else 'Validation summary: errors=0 warnings=0') in log, name
        numeric = 0
        if 'lighting' in view:
            folder = root/f'prompt19-{name}-lighting'; checks = read(folder/'readback.json')
            for key, record in view['lighting']['maps'].items():
                fmt, array = read_dds(Path(view['packageRoot'])/record['path'])
                assert (folder/(key+'.bin')).read_bytes() == array.tobytes(), (name, key)
                assert checks[key]['byteExact'] and checks[key]['nonfinite'] == 0
                assert checks[key]['stateBefore'] == checks[key]['stateAfter'] == 128 and checks[key]['stateCapture'] == 2048
                numeric += 1
        report['windows'][name] = {'configuration': 'Release' if name == 'release' else 'Debug',
            'frames': int(re.search(r'Completed frames=(\d+)', log)[1]), 'lightingMapsByteExact': numeric,
            'validation': 'disabled' if name == 'release' else {'errors': 0, 'warnings': 0}}
    for name in ('reload', 'reconstruct'):
        log = (root/f'prompt19-{name}.log').read_text(encoding='utf-8')
        assert log.count('Reconstruction Ready:') == 2 and info(name)['revision'] == 3
        assert log.count('Reconstruction stage lighting: complete') == 2
    assert 'Image debug switch: shading-proxy' in (root/'prompt19-warp-views.log').read_text(encoding='utf-8')
    assert 'failed GPU preparation retained' in (root/'prompt19-transaction.log').read_text(encoding='utf-8')
    assert 'cancelled completed upload retained' in (root/'prompt19-transaction.log').read_text(encoding='utf-8')
    for name in ('warp-views', 'ui'):
        log = (root/f'prompt19-{name}.log').read_text(encoding='utf-8')
        assert 'Resize: 960x540' in log and 'Resize: 1280x720' in log
    for file in (root/'scene18').rglob('*'):
        if file.is_file():
            relative = file.relative_to(root/'scene18')
            assert file.read_bytes() == (root/'scene19-final'/relative).read_bytes(), relative
            report['preservedFiles'].append(relative.as_posix())
    cases = read(root/'prompt19-final/synthetic-results.json')
    assert cases['lambert']['directionErrorDegrees'] <= 3
    for name in ('plane', 'black', 'overexposed', 'low-validity', 'neutral'):
        assert not cases[name]['fit']['identifiable'] and cases[name]['fit']['confidence'] <= .1
    report['synthetic'] = cases
    report['real'] = read(root/'scene19-final/lighting/lighting.json')['fit']
    fit_mask = read_dds(root/'scene19-final/lighting/fitMask.dds')[1] > 0
    for key, view in (('proxy', 'shading-proxy'), ('oldShading', 'old-shading'), ('residual', 'lighting-residual'), ('fitMask', 'fit-mask')):
        _, a = read_dds(root/f'scene19-final/lighting/{key}.dds')
        if key in ('proxy', 'oldShading'):
            expected = quantize(linear_to_srgb(a[..., :3]/4))
        elif key == 'residual':
            r = np.clip(a, 0, 1); expected = quantize(np.stack((r, .2*r, 1-r), -1))
        else:
            expected = np.repeat((a*255).astype(np.uint8)[..., None], 3, -1)
        if key in ('proxy', 'residual'): expected[~fit_mask] = [51, 8, 51]
        actual = pixels(capture('real-'+view)); error = np.abs(actual-expected.astype(np.int16))
        report['pixelChecks']['CPU preview '+key] = {'maxLSB': int(error.max())}
        assert error.max() <= 1, (key, error.max())
    write_json_atomic(root/'prompt19-verification.json', report)
    print(f"Verified {len(names)} windows, {len(report['preservedFiles'])} preserved files; byte-exact maps, finite/state checks and source/Final isolation")


if __name__ == '__main__':
    verify()
