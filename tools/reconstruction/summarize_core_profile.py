"""Summarize raw performance samples and resolution sensitivity without claiming ground truth."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image
from runtime_dds import read_dds


def run(root, prefix, output):
    model=json.loads((root/'profile.json').read_text(encoding='utf-8'))
    report=dict(models=[],frames=[],resolution={})
    for row in model['runs']:
        report['models'].append(dict(index=row['index'],analysisMaxSize=row['analysisMaxSize'],seconds=row['seconds'],
            backends=[{k:b[k] for k in ('backend','seconds','peakAllocatedBytes','peakReservedBytes') if k in b} for b in row['backends']],stages=row['stages']))
    for path in sorted(Path('generated').glob(prefix+'-*.profile.json')):
        data=json.loads(path.read_text(encoding='utf-8'))
        for key in ('cpuFrame',):
            d=data[key];a=np.asarray(d['samplesMs']);assert np.isfinite(a).all() and len(a)==d['count'] and d['count']>=300
            assert d['p95Ms']==sorted(a)[int(np.ceil(.95*len(a)))-1]
        report['frames'].append(dict(file=path.as_posix(),cpu={k:v for k,v in data['cpuFrame'].items() if k!='samplesMs'},
            gpu={k:v for k,v in data['gpuFrame']['distribution'].items() if k!='samplesMs'},
            update={k:v for k,v in data['imageUpdate']['distribution'].items() if k!='samplesMs'},
            memory={k:v for k,v in data['memory'].items() if k!='samples'},scene=data['scene']))
    packages=[root/r['finalPackage'] for r in model['runs']]
    anchors=[json.loads((p/'relighting/relighting.json').read_text(encoding='utf-8')) for p in packages]
    assert len({a['sourceImage']['sha256'] for a in anchors})==1
    assert all(a['sourceImage']['size']==anchors[0]['sourceImage']['size'] for a in anchors)
    high,low=packages[0],packages[-1]
    maps=lambda p,k:read_dds(p/'analysis'/(k+'.dds'))[1]
    hn,ln=maps(high,'normal')[...,:3],maps(low,'normal')[...,:3]
    h,w=hn.shape[:2];lh,lw=ln.shape[:2];ys=np.minimum(((np.arange(h)+.5)*lh/h).astype(int),lh-1);xs=np.minimum(((np.arange(w)+.5)*lw/w).astype(int),lw-1)
    up=lambda v:v[ys[:,None],xs[None,:]]
    valid=(maps(high,'validity')!=0)&(up(maps(low,'validity'))!=0)
    angle=np.degrees(np.arccos(np.clip(np.sum(hn*up(ln),axis=-1),-1,1)))[valid]
    hd,ld=maps(high,'depth'),up(maps(low,'depth'))
    report['resolution']=dict(anchorSize=anchors[0]['sourceImage']['size'],anchorHash=anchors[0]['sourceImage']['sha256'],
        analysisSizes=[a['analysisImage']['size'] for a in anchors],
        normalMeanDegrees=float(angle.mean()),normalP95Degrees=float(np.percentile(angle,95)),
        depthRelativeRmse=float(np.sqrt(np.mean(((hd[valid]-ld[valid])/hd[valid])**2))),
        meaning='cross-resolution sensitivity on common valid pixel centers using nearest lookup; NOT accuracy against ground truth')
    for p in (high,low):
        iid=json.loads((p/'intrinsic/intrinsic.json').read_text(encoding='utf-8'))
        light=json.loads((p/'lighting/lighting.json').read_text(encoding='utf-8'))
        report['resolution'][p.parent.name]=dict(fit=light['fit'],intrinsicMetrics=iid.get('metrics',{}))
    output.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='resolution'},indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--prefix',required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.root,a.prefix,a.output)
