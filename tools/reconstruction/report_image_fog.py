"""Assemble an evidence index and diagnostic contact sheet from actual Optional33 runs."""
import json
import math
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from verify_recipe import dds
from fog_reference import sample, evaluate


def report():
    root = Path('generated'); fog_root = root/'prompt33-fog'
    fog = json.loads((fog_root/'verification.json').read_text(encoding='utf-8'))
    result = {'fogMaximumError': max(v['maxFloatError'] for v in fog['cases'].values()),
              'fogCases': len(fog['cases']), 'profiles': {}, 'windows': [], 'coreVsPrevious': {}}
    for path in sorted(root.glob('prompt33-profile-*.profile.json')):
        data = json.loads(path.read_text(encoding='utf-8'))
        for key in ('cpuFrame', 'gpuFrame'):
            distribution = data[key] if key == 'cpuFrame' else data[key]['distribution']
            values = sorted(distribution['samplesMs']); assert len(values) == 360
            assert abs(values[math.ceil(.95*len(values))-1]-distribution['p95Ms']) < 1e-9
        record = {key: {k: v for k, v in data[key].items() if k not in ('samplesMs', 'samples')} for key in ('cpuFrame', 'memory', 'scene')}
        for key in ('gpuFrame', 'imageUpdate'):
            record[key] = {k: v for k, v in data[key]['distribution'].items() if k != 'samplesMs'}
            record[key]['changedFrames'] = data[key]['changedFrames']
        result['profiles'][path.stem] = record
    for path in sorted(root.glob('prompt33-*.log')):
        if path.stem.endswith('-run') or path.stem == 'prompt33-old3d':
            continue
        text = path.read_text(encoding='utf-8-sig', errors='replace')
        if not any(line.startswith('Adapter:') for line in text.splitlines()) or 'Completed frames=' not in text:
            continue
        validation = [line for line in text.splitlines() if line.startswith('Validation summary:')]
        assert validation and validation[-1] in ('Validation summary: errors=0 warnings=0', 'Validation summary: disabled (Release; messages not collected)'), path
        result['windows'].append({'log': str(path), 'validation': validation[-1]})
    for name in ('native', 'zero', 'edit', 'real-edit', 'protected', 'cycle'):
        a, b = root/'prompt32-demo-final'/name/'result.dds', root/'prompt33-core'/name/'result.dds'
        if a.exists() and b.exists():
            error = float(np.max(abs(dds(a)-dds(b)))); assert error == 0, (name, error)
            result['coreVsPrevious'][name] = {'floatMaxDifference': error}
    a, b = root/'prompt32-demo-final/real-edit/result.dds', root/'prompt33-v1/result.dds'
    if b.exists():
        error = float(np.max(abs(dds(a)-dds(b)))); assert error == 0
        result['v1RecipeMaxDifference'] = error
    for name in ('cancel', 'failed-reload'):
        path = root/('prompt33-'+name)/'result.dds'
        if path.exists():
            assert np.array_equal(dds(fog_root/'edge/result.dds'), dds(path)), name
            result[name+'Retained'] = True
    a, b = root/'prompt33-cast-baseline.bmp', root/'prompt33-cast-fog.bmp'
    if b.exists():
        assert np.array_equal(np.asarray(Image.open(a)), np.asarray(Image.open(b)))
        result['castDebugFogIsolation'] = True
    a, b = root/'prompt33-old3d.bmp', root/'prompt33-new3d.bmp'
    if b.exists():
        aa, bb = np.asarray(Image.open(a)), np.asarray(Image.open(b))
        error = int(np.max(abs(aa.astype(int)-bb.astype(int))))
        assert error == 0, ('matched 3D regression', error)
        result['scene3DMaxLsb'] = error
    # Numeric plots are explicitly CPU visualizations of GPU-readback distance, alongside the
    # actual GPU final. They are not substituted for the renderer's four live debug shaders.
    directory = fog_root/'edge'; meta = json.loads((directory/'export.json').read_text(encoding='utf-8'))
    view = json.loads((root/'prompt33-fog-edge.view.json').read_text(encoding='utf-8'))
    package = Path(view['packageRoot']); maps = json.loads((package/'analysis/analysis.json').read_text(encoding='utf-8'))
    distance = dds(directory/'fog-distance.dds'); baseline = dds(directory/'pre-fog.dds')
    labels = dds(package/maps['maps']['region']['path'])[..., 0]
    h, w = baseline.shape[:2]; samples = sample(distance, labels, w, h); settings = meta['composition']['fog']
    _, transmission, confidence = evaluate(baseline, samples, settings['density'], settings['airlightLinear'])
    distance_preview = samples[..., 0]/max(float(samples[..., 0].max()), 1e-6)
    airlight = np.asarray(settings['airlightLinear'])*(1-transmission[..., None])
    items = [('Original / fixed', Image.open(directory/'original.png').convert('RGB'))]
    for label, values in [('Ray distance / max', distance_preview), ('Effective transmission', transmission), ('Fog confidence', confidence), ('Airlight / linear', airlight)]:
        if values.ndim == 2:
            values = np.repeat(values[..., None], 3, -1)
        items.append((label, Image.fromarray(np.uint8(np.clip(values, 0, 1)*255))))
    items.append(('Result / GPU', Image.open(directory/'result.png').convert('RGB')))
    canvas = Image.new('RGB', (3*360, 2*310), (20, 24, 30)); draw = ImageDraw.Draw(canvas)
    for index, (label, im) in enumerate(items):
        x, y = (index%3)*360, (index//3)*310; im.thumbnail((344, 275)); canvas.paste(im, (x+8, y+28)); draw.text((x+8, y+7), label, fill='white')
    canvas.save(root/'prompt33-fog-overview.png')
    result['debugWindows'] = sum('errors=0' in w['validation'] for w in result['windows'])
    result['releaseWindows'] = len(result['windows'])-result['debugWindows']
    (root/'prompt33-summary.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'windows'}, indent=2))


if __name__ == '__main__':
    report()
