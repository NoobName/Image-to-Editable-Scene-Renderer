"""Ground-truth, feasibility, lifecycle and actual DX12 verification evidence for stage31."""
import argparse,json
from pathlib import Path
import numpy as np
from verify_recipe import dds
from pipeline.reference_backend import rgb_coefficients,LUMA

def verify(root,fixtures):
    root=Path(root);fixtures=Path(fixtures);truth=json.loads((fixtures/'truth.json').read_text(encoding='utf-8'))
    report={'tolerances':{'angleDegrees':3,'directAmbientRelative':.05,'cpuHlsl':5e-5,'warp':2e-5,'release':2e-6},'cases':{}}
    for name in ('optimized','different','warp','release','ui','cycle'):
        case=root/name;o=json.loads((case/'optimization.json').read_text(encoding='utf-8'));g=json.loads((case/'gpu-verification.json').read_text(encoding='utf-8'));e=json.loads((case/'export.json').read_text(encoding='utf-8'))
        assert o['status']=='improved' and o['bestLoss']<o['initialLoss']*.01,(name,o['status'])
        assert g['passed'] and g['maxAbsoluteError']<=5e-5 and g['nonfinite']==0 and g['before']==g['after']==128 and g['capture']==2048
        light=e['state']['target'];angle=float(np.degrees(np.arccos(np.clip(np.dot(light['direction'],truth['targetDirection']),-1,1))))
        direct,ambient=rgb_coefficients(light);ratio=float(direct@LUMA/(ambient@LUMA));expected=float(np.array(truth['direct'])@LUMA/(np.array(truth['ambient'])@LUMA));error=abs(ratio/expected-1)
        assert angle<=3 and error<=.05,(name,angle,error)
        assert e['state']['displayExposure']==0 and e['state']['sourceCalibration']==o['initialState']['sourceCalibration']
        assert all(a['bestLoss']>=b['bestLoss'] for a,b in zip(o['history'],o['history'][1:]))
        report['cases'][name]={'directionErrorDegrees':angle,'directAmbientRelativeError':error,'initialLoss':o['initialLoss'],'bestLoss':o['bestLoss'],'iterations':len(o['history']),'cpuGpuMaxError':g['maxAbsoluteError'],'seconds':o['seconds']}
    for name in ('inspect','reset','unregistered','protected','plane-reference','cancel','reload'):
        assert np.array_equal(dds(root/'baseline/result.dds'),dds(root/name/'result.dds')),name
    for name in ('reopen','ui','cycle'):
        assert np.array_equal(dds(root/'optimized/result.dds'),dds(root/name/'result.dds')),name
    assert np.array_equal(dds(root/'style-start/result.dds'),dds(root/'no-improvement/result.dds'))
    assert json.loads((root/'no-improvement/optimization.json').read_text(encoding='utf-8'))['status']=='no-improvement'
    edited=json.loads((root/'edited/export.json').read_text(encoding='utf-8'));assert abs(edited['state']['target']['directIntensity']-.123)<1e-7
    for name,tolerance in (('warp',2e-5),('release',2e-6)):
        error=float(np.max(np.abs(dds(root/'optimized/result.dds')-dds(root/name/'result.dds'))));assert error<=tolerance;report[name+'FinalMaxError']=error
    for path in root.glob('*/export.json'):
        for b in json.loads(path.read_text(encoding='utf-8'))['buffers'].values():assert b['nonfinite']==0 and b['before']==b['after']==128 and b['capture']==2048
        for file in path.parent.glob('*.dds'):dds(file)
    (root/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root');p.add_argument('fixtures');a=p.parse_args();verify(a.root,a.fixtures)
