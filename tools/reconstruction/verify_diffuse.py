"""Independent stage24 CPU/GPU checks; no image quality claims from visual plausibility."""
import json
from pathlib import Path
import numpy as np
from PIL import Image
from runtime_dds import read_dds
from stability_reference import sample,response
from verify_image_ratio import encode

def verify():
    root=Path('generated');results=[]
    for d in sorted(root.glob('prompt24-*-shading')):
        r=json.loads((d/'readback.json').read_text(encoding='utf-8'));p=r['composition']
        view=json.loads((root/(d.name.removesuffix('-shading')+'.view.json')).read_text(encoding='utf-8'))
        package=Path(view['packageRoot']);anchor=json.loads((package/'relighting/relighting.json').read_text(encoding='utf-8'))
        meta=json.loads((package/'analysis/analysis.json').read_text(encoding='utf-8'))
        source=np.asarray(Image.open(package/anchor['sourceImage']['path']).convert('RGB'),dtype=float);h,w=source.shape[:2]
        arrays={}
        for key in ('old','new','ratio','result','qualityA','qualityB'):
            rw,rh=r[key]['size'];arrays[key]=np.fromfile(d/(key+'.bin'),dtype='<f4').reshape(rh,rw,4).astype(float)
            assert np.isfinite(arrays[key]).all() and r[key]['stateBefore']==r[key]['stateAfter']==128 and r[key]['stateCapture']==2048
        def load(key):return read_dds(package/meta['maps'][key]['path'])[1]
        sampled,_=sample(arrays['old'],arrays['new'],arrays['qualityA'],arrays['qualityB'],load('depth'),load('normal'),load('region'),h,w,p)
        ratio,conf,out,_,_=response(*sampled,source,p)
        errors=[float(np.max(abs(arrays['ratio'][...,:3]-ratio))),float(np.max(abs(arrays['ratio'][...,3]-conf))),float(np.max(abs(arrays['result'][...,:3]-out)))]
        assert max(errors)<6e-5,(d,errors)
        if r['source']==r['target']:
            assert np.all(arrays['ratio'][...,:3]==1)
            assert np.max(abs(encode(arrays['result'][...,:3])-source))<=1
        # Verify newly filled quality channels against independently exported intrinsic floats.
        intrinsicPath=package/'intrinsic/intrinsic.json'
        qerr=0.
        if intrinsicPath.exists():
            iid=json.loads(intrinsicPath.read_text(encoding='utf-8'))
            maps={k:read_dds(package/v['path'])[1] for k,v in iid['maps'].items()}
            a,s=maps['albedo'][...,:3],maps['shading'][...,:3]
            residual=abs(maps['residual'][...,:3]) if 'residual' in maps else np.zeros_like(a)
            fraction=residual.max(-1)/((a*s+residual).max(-1)+.02)
            support=np.exp(-maps['error']/.15)*np.exp(-maps['uncertainty'][...,1]/.15)*np.clip(1-fraction/.5,0,1)
            support*= (maps['validity']!=0)&(a.min(-1)>=.03)
            qerr=float(np.max(abs(support-arrays['qualityB'][...,1])));assert qerr<3e-6
        results.append(dict(name=d.name,ratioConfidenceResultMaxError=errors,intrinsicSupportError=qerr))
    for bmp in root.glob('prompt24-*.bmp'):
        if 'release' not in bmp.stem:assert 'Validation summary: errors=0 warnings=0' in bmp.with_suffix('.log').read_text(encoding='utf-8-sig')
    assert (np.asarray(Image.open(root/'prompt24-final.bmp'))==np.asarray(Image.open(root/'prompt23-final.bmp'))).all()
    for name in ('indoor','outdoor','painting'):
        old=root/f'scene23-{name}';new=root/f'scene24-final-{name}'
        for folder in ('textures','intrinsic','meshes','relighting'):
            for f in (old/folder).rglob('*'):
                if f.is_file():assert f.read_bytes()==(new/f.relative_to(old)).read_bytes(),f
    old=root/'scene23-indoor';new=root/'scene24-final-indoor'
    light=json.loads((new/'lighting/lighting.json').read_text(encoding='utf-8'))
    observed=read_dds(new/'intrinsic/shading.dds')[1][...,:3]/light['assistance']['calibrationScale']
    before=read_dds(old/'lighting/oldShading.dds')[1][...,:3];after=read_dds(new/'lighting/oldShading.dds')[1][...,:3]
    mask=read_dds(new/'lighting/fitMask.dds')[1]!=0
    delta=np.sqrt(np.mean((after-observed)**2,-1))-np.sqrt(np.mean((before-observed)**2,-1))
    distribution={key:int((mask&condition).sum()) for key,condition in [('improved',delta<-.01),('tie',abs(delta)<=.01),('degraded',delta>.01)]}
    report=dict(buffers=results,nonfinite=0,originalAssetsUnchanged=True,final3DByteExact=True,realIndoorSupportedPixelComparison=distribution)
    (root/'prompt24-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
if __name__=='__main__':verify()
