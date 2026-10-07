"""Verify newly rendered core-demo evidence, never substitute historical screenshots."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image
from verify_recipe import dds


def verify(output, inputs):
    report = dict(tolerances=dict(identityLsb=1, replayLsb=0, warpFloat=2e-5), comparisons={})
    rgb = lambda case, name='result': np.asarray(Image.open(output/case/(name+'.png')).convert('RGB'), dtype=int)
    for case in ('native', 'zero', 'cycle', 'transaction', 'cancel', 'missing', 'protected', 'black', 'emission', 'real-original'):
        if not (output/case).is_dir():
            continue
        a, b = rgb(case), rgb(case, 'original')
        error = int(np.abs(a-b).max())
        assert error <= 1, (case, error)
        report['comparisons'][case+'Identity'] = dict(maxLsb=error, size=[a.shape[1], a.shape[0]])
    assert rgb('native').shape == (1000, 1500, 3)
    for a, b in [('edit','reopen'), ('edit','ui'), ('edit','reload'), ('reference','reference-reopen'),
                 ('real-edit','real-reopen'), ('real-edit','real-cycle')]:
        if not (output/a).is_dir():
            continue
        error = int(np.abs(rgb(a)-rgb(b)).max())
        assert error == 0, (a,b,error)
        for key in ('result','ratio','old-shading','new-shading','confidence'):
            assert np.array_equal(dds(output/a/(key+'.dds')), dds(output/b/(key+'.dds'))), (a,b,key)
        report['comparisons'][a+'->'+b] = dict(maxLsb=error, floatExact=True)
    for case in ('warp','release'):
        error = float(np.abs(dds(output/'edit/result.dds')-dds(output/case/'result.dds')).max())
        assert error <= 2e-5, (case,error)
        report['comparisons'][case] = dict(maxFloat=error)
    source_hashes = set()
    for path in output.glob('*/export.json'):
        data = json.loads(path.read_text(encoding='utf-8'))
        view=json.loads((Path('generated')/(output.name+'-'+path.parent.name+'.view.json')).read_text(encoding='utf-8'))
        package=Path(view['packageRoot'])
        anchor=json.loads((package/'relighting/relighting.json').read_text(encoding='utf-8'))['sourceImage']
        assert hashlib.sha256((package/anchor['path']).read_bytes()).hexdigest() == data['anchorSha256'] == anchor['sha256']
        # WIC may encode a different PNG byte stream. Pixel identity and asset-file identity
        # are separate checks; recompressed PNG bytes must not be mistaken for changed RGB.
        assert np.array_equal(rgb(path.parent.name,'original'),np.asarray(Image.open(package/anchor['path']).convert('RGB'),dtype=int))
        source_hashes.add(data['anchorSha256'])
        for buffer in data['buffers'].values():
            assert buffer['nonfinite']==0 and buffer['before']==buffer['after']==128 and buffer['capture']==2048
        for texture in path.parent.glob('*.dds'):
            dds(texture)
    change = np.abs(dds(output/'reference/new-shading.dds')-dds(output/'reference/old-shading.dds'))[...,:3]
    assert change.mean()>.1
    report['referenceMeanShadingChange'] = float(change.mean())
    report['uniqueAnchors'] = len(source_hashes)
    report['sourceIntegrity'] = 'package anchor file SHA-256 verified; exported original decoded pixels exact; numerical buffers finite and SRV states restored'
    (output/'verification.json').write_text(json.dumps(report,indent=2), encoding='utf-8')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--inputs',type=Path,required=True)
    a=p.parse_args();verify(a.output,a.inputs)
