"""Independent float oracle for every captured stage22 pass, including guarded upsampling."""
import json
from pathlib import Path
import numpy as np
from PIL import Image
from runtime_dds import read_dds
from stability_reference import sample,response
from verify_image_ratio import encode


def verify(root=Path('generated')):
    results=[]
    for d in sorted(root.glob('prompt22-*-shading')):
        r=json.loads((d/'readback.json').read_text(encoding='utf-8'));p=r['composition']
        view=json.loads((root/(d.name.removesuffix('-shading')+'.view.json')).read_text(encoding='utf-8'))
        package=Path(view['packageRoot']);meta=json.loads((package/'analysis/analysis.json').read_text(encoding='utf-8'))
        anchor=json.loads((package/'relighting/relighting.json').read_text(encoding='utf-8'))
        source=np.asarray(Image.open(package/anchor['sourceImage']['path']).convert('RGB'),dtype=float);h,w=source.shape[:2]
        arrays={}
        for key in ('old','new','ratio','result','qualityA','qualityB'):
            rw,rh=r[key]['size'];arrays[key]=np.fromfile(d/(key+'.bin'),dtype='<f4').reshape(rh,rw,4).astype(float)
            assert np.isfinite(arrays[key]).all() and r[key]['stateBefore']==r[key]['stateAfter']==128 and r[key]['stateCapture']==2048
        def load(name):return read_dds(package/meta['maps'][name]['path'])[1].astype(float) if name!='region' else read_dds(package/meta['maps'][name]['path'])[1]
        sampled,guard=sample(arrays['old'],arrays['new'],arrays['qualityA'],arrays['qualityB'],load('depth'),load('normal'),load('region'),h,w,p)
        mask=0
        if p['protection']:
            raw=np.asarray(Image.open(p['protection']['path']).convert('L'),dtype=float)/255;mh,mw=raw.shape
            mask=raw[np.floor((np.arange(h)+.5)*mh/h).astype(int)[:,None],np.floor((np.arange(w)+.5)*mw/w).astype(int)]
        ratio,conf,out,_,_=response(*sampled,source,p,mask)
        error=float(np.max(np.abs(arrays['ratio'][...,:3]-ratio)));ce=float(np.max(np.abs(arrays['ratio'][...,3]-conf)));oe=float(np.max(np.abs(arrays['result'][...,:3]-out)))
        assert max(error,ce,oe)<6e-5,(d,error,ce,oe)
        identity=(r['source']==r['target'] or p['strength']==0 or np.all(conf==0));lsb=None
        if identity:
            lsb=int(np.max(np.abs(encode(arrays['result'][...,:3])-source)));assert lsb<=1 and np.all(arrays['ratio'][...,:3]==1)
        protected=conf==0
        assert np.all(arrays['ratio'][...,:3][protected]==1),(d.name,float(np.max(np.abs(arrays['ratio'][...,:3][protected]-1))),float(np.max(arrays['ratio'][...,3][protected])))
        unchanged_lsb=int(np.max(np.abs(encode(arrays['result'][...,:3])[protected]-source[protected]))) if protected.any() else 0
        assert unchanged_lsb<=1
        trusted=(conf>.7)&(~protected);change=float(np.mean(np.abs(ratio[trusted]-1))) if trusted.any() else 0
        if 'fixture' in str(package) and 'preset' in d.name:assert change>.15,(d,change)
        results.append(dict(name=d.name,ratioError=error,confidenceError=ce,resultError=oe,identityLSB=lsb,protectedLSB=unchanged_lsb,trustedMeanChange=change,guardedFraction=float(guard.mean()),gpuTiming=r['gpuTiming']))
    for path in root.glob('prompt22-*.bmp'):
        if 'release' not in path.stem:assert 'Validation summary: errors=0 warnings=0' in path.with_suffix('.log').read_text(encoding='utf-8-sig')
    # Repeat deterministic target trajectory: history is never a shading input.
    for key in ('ratio','result'):
        a=(root/f'prompt22-drag-shading/{key}.bin').read_bytes();b=(root/f'prompt22-drag-repeat-shading/{key}.bin').read_bytes();assert a==b,key
    final=np.asarray(Image.open(root/'prompt22-final.bmp'),dtype=int);baseline=np.asarray(Image.open(root/'prompt21-final.bmp'),dtype=int);assert np.max(abs(final-baseline))==0
    output=dict(buffers=results,nonfinite=0,dragRepeatByteExact=True,finalMaxLSB=0)
    (root/'prompt22-verification.json').write_text(json.dumps(output,indent=2),encoding='utf-8');print(json.dumps(output,indent=2))


if __name__=='__main__':verify()
