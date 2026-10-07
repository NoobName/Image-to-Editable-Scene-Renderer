"""Stage28 numerical UI contract checks; no inference or image synthesis.

Tolerance: protected RGB32F <= 2e-7 from explicit source sRGB decode.
Clear/exposure/overlay must leave linear composition byte-identical.
"""
import json
import re
from pathlib import Path
import numpy as np
from PIL import Image


def read(name):
    directory = Path('generated') / (name + '-shading')
    meta = json.loads((directory / 'readback.json').read_text(encoding='utf-8'))
    maps = {}
    for path in directory.glob('*.bin'):
        item = meta[path.stem]
        assert item['stateBefore'] == item['stateAfter'] == 128 and item['stateCapture'] == 2048
        assert item['nonfinite'] == 0
        w, h = item['size']
        maps[path.stem] = np.fromfile(path, np.float32).reshape(h, w, -1)
        assert np.isfinite(maps[path.stem]).all(), path
    return meta, maps


def main():
    root = Path('generated')
    _, baseline = read('prompt28-baseline')
    meta, protected = read('prompt28-workspace-protect')
    package = root / 'prompt27-final-fixtures/plane'
    anchor = json.loads((package / 'relighting/relighting.json').read_text(encoding='utf-8'))
    rgb = np.asarray(Image.open(package / anchor['sourceImage']['path']).convert('RGB'), np.float64) / 255
    linear = np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)
    mask = protected['protection'][..., 0] == 1
    assert 0 < mask.sum() < mask.size
    error = float(np.max(np.abs(protected['result'][mask, :3] - linear[mask])))
    assert error < 2e-7, error
    assert np.array_equal(protected['result'][~mask], baseline['result'][~mask])
    for name in ('workspace-clear', 'workspace-exposure', 'workspace-overlay', 'release'):
        _, maps = read('prompt28-' + name)
        assert np.array_equal(maps['result'], baseline['result']), name
    _, warp = read('prompt28-protect-warp')
    warp_error = float(np.max(np.abs(warp['result'] - protected['result'])))
    assert warp_error < 2e-5
    _, global_gain = read('prompt28-workspace-global')
    assert np.array_equal(global_gain['old'], baseline['old'])
    assert np.allclose(global_gain['new'][..., :3], baseline['new'][..., :3] * 2, atol=2e-7)
    assert not np.array_equal(global_gain['result'], baseline['result'])
    assert (root / 'prompt28-3d.bmp').read_bytes() == (root / 'prompt27-3d.bmp').read_bytes()
    if (root / 'prompt28-debug-depth-base.bmp').exists():
        assert (root / 'prompt28-debug-depth.bmp').read_bytes() == (root / 'prompt28-debug-depth-base.bmp').read_bytes()
    for name in ('workspace-exposure', 'workspace-overlay'):
        assert (root / ('prompt28-' + name + '.bmp')).read_bytes() != (root / 'prompt28-baseline.bmp').read_bytes()
    entries = []
    for directory in sorted(root.glob('prompt28-*-shading')):
        report, maps = read(directory.name.removesuffix('-shading'))
        entries.append(dict(name=directory.name, maps=len(maps), finite=True, statesRestored=True))
    logs = []
    for path in sorted(root.glob('prompt28-*.log')):
        text = path.read_text(encoding='utf-8-sig', errors='replace')
        if 'Saved GPU frame capture:' not in text or 'smoke PID=' in text:
            continue
        release = path.stem == 'prompt28-release'
        assert release or 'Validation summary: errors=0 warnings=0' in text, path
        slots = re.findall(r'freeFontSlots=(\d+)', text)
        # v1.92 creates its first font texture lazily after the initial source bind.
        # One initial 62 -> 61 transition is expected; subsequent document binds must stay at 61.
        assert not slots or (slots[0] in ('61', '62') and all(v == '61' for v in slots[1:])), (path, slots)
        logs.append(dict(name=path.name, debug=not release, freeFontSlots=slots))
    result = dict(protectedPixels=int(mask.sum()), totalPixels=mask.size, protectedMaxError=error,
                  warpMaxError=warp_error, unprotectedUnchanged=True, clearExact=True,
                  exposureAndOverlayPresentationOnly=True, threeDExact=True, captures=entries, windows=logs)
    (root / 'prompt28-verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('captures', 'windows')}, indent=2))
    print(f'Verified {len(entries)} readbacks and {len(logs)} finite windows')


if __name__ == '__main__':
    main()
