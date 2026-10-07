"""Check stage 20 captured float buffers against the independent Python evaluator."""
import json
from pathlib import Path
import numpy as np
from PIL import Image
from runtime_dds import read_dds
from shading_reference import evaluate


def verify(root=Path('generated')):
    results = []
    for directory in sorted(root.glob('prompt20-*-shading')):
        name = directory.name.removesuffix('-shading')
        r = json.loads((directory / 'readback.json').read_text(encoding='utf-8'))
        v = json.loads((root / f'{name}.view.json').read_text(encoding='utf-8'))
        package = Path(v['packageRoot'])
        errors = {}
        w, h = r['size']
        for key, light in [('old', 'source'), ('new', 'target')]:
            actual = np.fromfile(directory / f'{key}.bin', dtype='<f4').reshape(h, w, 4)
            assert np.isfinite(actual).all()
            if r['available']:
                normal = read_dds(package / 'analysis/normal.dds')[1]
                validity = read_dds(package / 'analysis/validity.dds')[1]
                expected = evaluate(normal, validity, r[light])
            else:
                expected = np.zeros_like(actual)
            errors[key] = float(np.max(np.abs(actual - expected)))
            assert errors[key] < 2e-5, (name, key, errors[key])
            assert r[key]['stateBefore'] == r[key]['stateAfter'] == 128
            assert r[key]['stateCapture'] == 2048
        if r['source'] == r['target']:
            assert (directory / 'old.bin').read_bytes() == (directory / 'new.bin').read_bytes()
        if name == 'prompt20-target':
            assert r['oldUpdates'] == 1 and r['newUpdates'] == 2
        if name == 'prompt20-source':
            assert r['oldUpdates'] == 2 and r['newUpdates'] == 2
        results.append({'name': name, 'maxError': errors, 'oldUpdates': r['oldUpdates'], 'newUpdates': r['newUpdates']})
    assert len(results) == 18, len(results)
    for path in root.glob('prompt20-*.bmp'):
        log = path.with_suffix('.log').read_text(encoding='utf-8-sig')
        if path.stem != 'prompt20-release':
            assert 'Validation summary: errors=0 warnings=0' in log, path
    source = np.asarray(Image.open(root / 'scene19-final/textures/source_anchor.png').convert('RGB'), dtype=int)
    actual = np.asarray(Image.open(root / 'prompt20-original.bmp').convert('RGB'), dtype=int)
    source_error = int(np.max(np.abs(actual-source)))
    assert source_error <= 1
    before = np.asarray(Image.open(root / 'prompt19-after-final.bmp').convert('RGB'), dtype=int)
    after = np.asarray(Image.open(root / 'prompt20-final.bmp').convert('RGB'), dtype=int)
    final_error = int(np.max(np.abs(before-after))); assert final_error == 0
    report = {'windows': len(list(root.glob('prompt20-*.bmp'))), 'buffers': results,
              'sourceMaxLSB': source_error, 'finalMaxLSB': final_error, 'nonfinite': 0}
    (root / 'prompt20-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    verify()
