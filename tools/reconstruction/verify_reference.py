"""Independent stage30 GPU checks: fixed source/EV, shading changes, recipe replay and WARP."""
import argparse,json
from pathlib import Path
import numpy as np
from verify_recipe import dds
from pipeline.reference_backend import rgb_coefficients

def verify(root,fixtures):
    root=Path(root);fixtures=Path(fixtures);report={'tolerances':{'sameBuild':0,'releaseFloat':2e-6,'releaseLsb':0,'warp':2e-5,'cpuHlsl':5e-5},'comparisons':{}}
    for name in ('inspect','reset','plane','fallback','cancel','missing'):
        assert np.array_equal(dds(root/'baseline/result.dds'),dds(root/name/'result.dds')),name
        report['comparisons'][name]='baseline exact'
    for name in ('process','ui-repeat','narrow','reopen'):
        assert np.array_equal(dds(root/'apply/result.dds'),dds(root/name/'result.dds')),name
        report['comparisons'][name]='applied result exact'
    warp=float(np.max(np.abs(dds(root/'apply/result.dds')-dds(root/'warp/result.dds'))));assert warp<=2e-5;report['warpMaxError']=warp
    error=float(np.max(np.abs(dds(root/'apply/result.dds')-dds(root/'release/result.dds'))));assert error<=2e-6;report['releaseMaxError']=error
    from PIL import Image
    assert np.array_equal(np.asarray(Image.open(root/'apply/result.png')),np.asarray(Image.open(root/'release/result.png')))
    old=dds(root/'baseline/old-shading.dds');new=dds(root/'apply/new-shading.dds')
    assert np.array_equal(old,dds(root/'apply/old-shading.dds'))
    report['newShadingMeanChange']=float(np.mean(np.abs(new[...,:3]-old[...,:3])));assert report['newShadingMeanChange']>.05
    normal=dds(fixtures/'source/analysis/runtime/normal-camera.dds') if (fixtures/'source/analysis/runtime/normal-camera.dds').exists() else None
    proposal=json.loads((fixtures/'reference-proposal/reference.json').read_text(encoding='utf-8'))
    if normal is None:
        # Reference and source synthetic fixtures deliberately share the normal field.
        normal=dds(fixtures/'reference-proposal/debug/normal.dds')
    light=proposal['proposal']['target'];d,b=rgb_coefficients(light);n=normal[...,:3].astype(np.float64);n/=np.maximum(np.linalg.norm(n,axis=-1,keepdims=True),1e-8)
    expected=b+np.maximum(n@-np.asarray(light['direction']),0)[...,None]*d
    error=float(np.max(np.abs(expected-new[...,:3])));assert error<=5e-5,error;report['cpuHlslMaxError']=error
    recipe=json.loads((root/'matched.json').read_text(encoding='utf-8'));assert recipe['state']['displayExposure']==0
    for path in root.glob('*/export.json'):
        metadata=json.loads(path.read_text(encoding='utf-8'))
        for buffer in metadata['buffers'].values():assert buffer['nonfinite']==0 and buffer['before']==buffer['after']==128 and buffer['capture']==2048
        for data in path.parent.glob('*.dds'):dds(data)
    (root/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root');p.add_argument('fixtures');a=p.parse_args();verify(a.root,a.fixtures)
