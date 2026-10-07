"""Verify current GPU data, guarded boundaries, immutable anchors and native recipe exports."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image
from verify_recipe import dds
from fog_reference import sample, evaluate


def verify(root):
    root = Path(root); report = {'tolerance': 3e-6, 'cases': {}}
    for directory in sorted(root.iterdir()):
        if not (directory/'export.json').exists():
            continue
        data = json.loads((directory/'export.json').read_text(encoding='utf-8'))
        fog = data['composition']['fog']; view = json.loads((Path('generated')/(root.name+'-'+directory.name+'.view.json')).read_text(encoding='utf-8'))
        package = Path(view['packageRoot'])
        actual, baseline, distance = dds(directory/'result.dds'), dds(directory/'pre-fog.dds'), dds(directory/'fog-distance.dds')
        ah, aw = distance.shape[:2]; h, w = baseline.shape[:2]
        labels = np.zeros((ah, aw), np.uint32)
        if fog['available']:
            manifest = json.loads((package/'analysis/analysis.json').read_text(encoding='utf-8'))
            labels = dds(package/manifest['maps']['region']['path'])[..., 0]
            point = dds(package/manifest['maps']['position']['path'])[..., :3]
            valid = distance[..., 3] > 0
            ray_error = float(np.max(abs(np.linalg.norm(point.astype(float), axis=-1)[valid]-distance[..., 0][valid])))
            assert ray_error < 2e-6, (directory, ray_error)
        else:
            ray_error = 0
        samples = sample(distance, labels, w, h)
        raw = Path('generated')/(root.name+'-'+directory.name+'-shading/protection.bin')
        protection = np.frombuffer(raw.read_bytes(), '<f4').reshape(h, w)
        expected, transmission, confidence = evaluate(baseline, samples, fog['density'], fog['airlightLinear'], protection, fog['active'])
        error = float(np.max(abs(expected-actual[..., :3])))
        assert error <= 3e-6, (directory, error)
        protected = confidence == 0
        assert np.array_equal(actual[..., :3][protected], baseline[..., :3][protected]), (directory, 'protected pixels changed')
        for buffer in data['buffers'].values():
            assert buffer['nonfinite'] == 0 and buffer['before'] == buffer['after'] == 128 and buffer['capture'] == 2048
        original = np.asarray(Image.open(directory/'original.png').convert('RGB'))
        assert (w, h) == tuple(data['nativeSize']) and original.shape[:2] == (h, w)
        exposed = actual[..., :3].astype(float)*2**data['displayExposure']
        encoded = np.where(exposed <= .0031308, exposed*12.92, 1.055*np.maximum(exposed, 0)**(1/2.4)-.055)
        png = np.asarray(Image.open(directory/'result.png').convert('RGB'), dtype=int)
        png_error = int(np.max(abs(png-np.floor(np.clip(encoded, 0, 1)*255+.5).astype(int))))
        assert png_error <= 1, (directory, 'display exposure/encoding', png_error)
        report['cases'][directory.name] = {'maxFloatError': error, 'rayDistanceError': ray_error, 'active': fog['active'],
            'transmittanceRange': [float(transmission.min()), float(transmission.max())], 'protectedPixels': int(protected.sum()), 'size': [w, h], 'displayPngMaxLsb': png_error}
    for a, b in [('edge', 'reopen'), ('edge', 'cycle'), ('edge', 'ui'), ('edge', 'reload')]:
        if (root/b/'result.dds').exists():
            assert np.array_equal(dds(root/a/'result.dds'), dds(root/b/'result.dds')), (a, b)
    if (root/'warp/result.dds').exists():
        error = float(np.max(abs(dds(root/'edge/result.dds')-dds(root/'warp/result.dds')))); assert error < 3e-6
        report['warpDifference'] = error
    (root/'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8'); print(json.dumps(report, indent=2))


if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('output'); verify(p.parse_args().output)
