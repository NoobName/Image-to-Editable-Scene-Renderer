"""Readback oracle for paired GGX, source separation and bounded additive changes."""
import json
from pathlib import Path
import numpy as np
from PIL import Image
from runtime_dds import read_dds
from stability_reference import sample,response,smooth,LUMA
from verify_image_ratio import decode,encode
from specular_reference import ggx

def verify():
    root=Path('generated');results=[]
    for d in sorted(root.glob('prompt25-*-shading')):
        r=json.loads((d/'readback.json').read_text(encoding='utf-8'));p=r['composition'];view=json.loads((root/(d.name.removesuffix('-shading')+'.view.json')).read_text(encoding='utf-8'))
        package=Path(view['packageRoot']);anchor=json.loads((package/'relighting/relighting.json').read_text(encoding='utf-8'));meta=json.loads((package/'analysis/analysis.json').read_text(encoding='utf-8'))
        source=np.asarray(Image.open(package/anchor['sourceImage']['path']).convert('RGB'),dtype=float);h,w=source.shape[:2];linear=decode(source/255)
        keys=('old','new','ratio','result','qualityA','qualityB','specularOld','specularNew','specularCandidate','specularProtected');a={}
        for key in keys:
            rw,rh=r[key]['size'];a[key]=np.fromfile(d/(key+'.bin'),dtype='<f4').reshape(rh,rw,4).astype(float)
            assert np.isfinite(a[key]).all() and r[key]['stateBefore']==r[key]['stateAfter']==128 and r[key]['stateCapture']==2048
        def load(key):return read_dds(package/meta['maps'][key]['path'])[1].astype(float)
        normal=load('normal');position=load('position');albedo=load('albedo');rough=load('roughness');metal=load('metallic');valid=load('validity')!=0
        ggxError=0.
        if p['specular']['available']:
            for which,key,rs in [('source','specularOld',1),('target','specularNew',p['specularRoughnessScale'])]:
                light=r[which];expected=ggx(normal[...,:3],position[...,:3],albedo[...,:3],np.clip(rough*rs,.045,1),metal,light['direction'],np.array(light['directColor'])*light['directIntensity'])*p['specular']['scale'];expected[~valid]=0
                difference=abs(expected-a[key][...,:3]);err=float(difference.max());ggxError=max(ggxError,err)
                # GGX can exceed RGB1 by orders of magnitude. Float32 dot products near the peak
                # need a relative tolerance; the final bounded image is still tested absolutely.
                assert np.all(difference<3e-4+3e-4*abs(expected)),(d,key,err)
        sampled,_=sample(a['old'],a['new'],a['qualityA'],a['qualityB'],load('depth'),normal,load('region'),h,w,p)
        mask=0
        if p['protection']:
            raw=np.asarray(Image.open(p['protection']['path']).convert('L'),dtype=float)/255;mh,mw=raw.shape
            mask=raw[np.floor((np.arange(h)+.5)*mh/h).astype(int)[:,None],np.floor((np.arange(w)+.5)*mw/w).astype(int)]
        ratio,confidence,baseline,_,_=response(*sampled,source,p,mask);expected=baseline.copy();candidate=np.zeros_like(linear);delta=np.zeros_like(linear)
        ah,aw=normal.shape[:2];cx,cy=np.meshgrid(((np.arange(w)+.5)*aw/w).astype(int),((np.arange(h)+.5)*ah/h).astype(int))
        raw=a['specularCandidate'][cy,cx];old=a['specularOld'][cy,cx,:3];new=a['specularNew'][cy,cx,:3]
        if p['specular']['available'] and p['specularEnabled'] and p['fitCacheValid'] and r['available']:
            signal=smooth(.002,.04,linear@LUMA)*(1-smooth(.92,.98,(source/255).max(-1)))
            amount=p['specularStrength']*p['strength']*(1-mask)*signal*np.clip(sampled[2][...,0],0,1)
            candidate=np.minimum(raw[...,:3]*amount[...,None],.6*linear)
            change=(new-old)*(amount*raw[...,3])[...,None]
            delta=np.clip(change,-candidate,p['specularMaxDelta']*linear+.02*amount[...,None])
            expected+=candidate*(1-ratio)+delta
        error=float(np.max(abs(a['result'][...,:3]-expected)));assert error<8e-5,(d,error)
        assert (a['result'][...,:3]>=-1e-7).all()
        recomposition=float(np.max(abs((linear-candidate)+candidate-linear)));assert recomposition<1e-15
        protected=raw[...,3]==0
        assert np.max(abs(a['result'][...,:3][protected]-baseline[protected]),initial=0)<8e-5
        identity=r['source']==r['target'] and p['specularRoughnessScale']==1
        lsb=None
        if identity or np.all(mask==1):
            lsb=int(np.max(abs(encode(a['result'][...,:3])-source)));assert lsb<=1
        results.append(dict(name=d.name,ggxMaxError=ggxError,compositionMaxError=error,recompositionError=recomposition,identityLSB=lsb,supportedPixels=p['specular'].get('supportedPixels',0),maxClip=p['specular'].get('maxClip',0)))
    for shape in ('plane','sphere'):
        def arrays(mode,key):
            path=root/f'prompt25-{shape}-{mode}-shading';info=json.loads((path/'readback.json').read_text(encoding='utf-8'));w,h=info[key]['size'];return np.fromfile(path/(key+'.bin'),dtype='<f4').reshape(h,w,4)
        old=arrays('specular-move','specularOld')[...,:3];new=arrays('specular-move','specularNew')[...,:3];wide=arrays('specular-rough','specularNew')[...,:3]
        assert np.argmax(old[...,0])!=np.argmax(new[...,0]);assert wide[...,0].max()<new[...,0].max()
        oldPixel=np.unravel_index(np.argmax(old[...,0]),old.shape[:2]);newPixel=np.unravel_index(np.argmax(new[...,0]),new.shape[:2])
        changed=arrays('specular-move','result')-arrays('specular-off','result')
        assert changed[oldPixel][0]<0,(shape,oldPixel,changed[oldPixel]);assert changed[newPixel][0]>0,(shape,newPixel,changed[newPixel])
    for key in ('ratio','result'):
        assert (root/f'prompt25-disabled-baseline-shading/{key}.bin').read_bytes()==(root/f'prompt24-ratio-color-shading/{key}.bin').read_bytes(),key
    assert np.array_equal(np.asarray(Image.open(root/'prompt25-final.bmp')),np.asarray(Image.open(root/'prompt24-final.bmp')))
    for bmp in root.glob('prompt25-*.bmp'):
        if 'release' not in bmp.stem:assert 'Validation summary: errors=0 warnings=0' in bmp.with_suffix('.log').read_text(encoding='utf-8-sig')
    path=root/'prompt25-real-protected-shading';info=json.loads((path/'readback.json').read_text(encoding='utf-8'));w,h=info['result']['size']
    actual=np.fromfile(path/'result.bin',dtype='<f4').reshape(h,w,4)[...,:3]
    original=decode(np.asarray(Image.open(root/'scene24-final-indoor/textures/source_anchor.png').convert('RGB'),dtype=float)/255)
    boxes=json.loads((root/'prompt25-fixtures/real-regions.json').read_text(encoding='utf-8'))['regions'];regions={}
    for key,(x0,y0,x1,y1) in boxes.items():
        error=float(np.max(abs(actual[y0:y1,x0:x1]-original[y0:y1,x0:x1])))
        if key!='painted_door_comparison':assert error<1e-6
        regions[key]={'maxChangeFromOriginal':error,'manualProtection':key!='painted_door_comparison'}
    report=dict(buffers=results,nonfinite=0,disabledRestores24ByteExact=True,final3DByteExact=True,syntheticPeakMovementAndRoughness=True,realManuallyMarkedRegions=regions)
    (root/'prompt25-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
if __name__=='__main__':verify()
