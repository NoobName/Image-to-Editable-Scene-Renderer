"""Verify source-domain identity, detail preservation and captured ratio math."""
import json
from pathlib import Path
import numpy as np
from PIL import Image


def decode(x):
    return np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)


def encode(x):
    return np.rint(np.clip(np.where(x<=.0031308,x*12.92,1.055*np.maximum(x,0)**(1/2.4)-.055),0,1)*255)


def ratio_reference(old,new,params):
    luma=np.array([.2126,.7152,.0722]);e=params['epsilon'];valid=(old[...,3]*new[...,3])!=0
    y=(new[...,:3]@luma+e)/(old[...,:3]@luma+e)
    r=y[...,None]*np.ones(3)
    if params['colorMode']=='bounded-color':
        r*=np.clip(((new[...,:3]+e)/(old[...,:3]+e))/y[...,None],.5,2)
    r=np.clip(r,*params['ratioClamp'])**params['strength']
    return np.where(valid[...,None],r,1),valid


def verify(root=Path('generated')):
    results=[]
    for d in sorted(root.glob('prompt21-*-shading')):
        r=json.loads((d/'readback.json').read_text(encoding='utf-8'));c=r['composition']
        v=json.loads((root/(d.name.removesuffix('-shading')+'.view.json')).read_text(encoding='utf-8'))
        package=Path(v['packageRoot']);anchor=json.loads((package/'relighting/relighting.json').read_text(encoding='utf-8'))
        source=np.asarray(Image.open(package/anchor['sourceImage']['path']).convert('RGB'),dtype=np.float64)
        h,w=source.shape[:2];aw,ah=r['size']
        old=np.fromfile(d/'old.bin',dtype='<f4').reshape(ah,aw,4).astype(float)
        new=np.fromfile(d/'new.bin',dtype='<f4').reshape(ah,aw,4).astype(float)
        ratio=np.fromfile(d/'ratio.bin',dtype='<f4').reshape(h,w,4);image=np.fromfile(d/'result.bin',dtype='<f4').reshape(h,w,4)
        expected,valid=ratio_reference(old,new,c)
        ax=np.floor((np.arange(w)+.5)*aw/w).astype(int);ay=np.floor((np.arange(h)+.5)*ah/h).astype(int)
        expected=expected[ay[:,None],ax];valid=valid[ay[:,None],ax]
        ratio_error=float(np.max(np.abs(ratio[...,:3]-expected)))
        image_error=float(np.max(np.abs(image[...,:3]-decode(source/255)*expected)))
        assert ratio_error<5e-5 and image_error<5e-5,(d,ratio_error,image_error)
        assert np.isfinite(image).all() and np.isfinite(ratio).all()
        for key in ('old','new','ratio','result'):
            assert r[key]['stateBefore']==r[key]['stateAfter']==128 and r[key]['stateCapture']==2048
        identity=r['source']==r['target'] or c['strength']==0
        lsb=int(np.max(np.abs(encode(image[...,:3])-source))) if identity else None
        if identity:assert lsb<=1 and np.all(ratio[...,:3]==1),(d.name,lsb,float(np.max(np.abs(ratio[...,:3]-1))))
        invalid_lsb=int(np.max(np.abs(encode(image[...,:3])[~valid]-source[~valid]))) if np.any(~valid) else None
        if invalid_lsb is not None:assert invalid_lsb<=1 and np.all(ratio[...,:3][~valid]==1)
        results.append({'name':d.name,'ratioError':ratio_error,'imageError':image_error,'identityLSB':lsb,'invalidLSB':invalid_lsb,'gpuTiming':r['gpuTiming']})
    assert len(results)==21,len(results)
    for name in ('ratio-reset','ratio-zero','detail-identity','cycle','transaction','failed'):
        v=json.loads((root/f'prompt21-{name}.view.json').read_text(encoding='utf-8'));package=Path(v['packageRoot'])
        anchor=json.loads((package/'relighting/relighting.json').read_text(encoding='utf-8'))
        if name in ('failed',):continue # non-native capture, validated numerical identity above
        image=np.asarray(Image.open(root/f'prompt21-{name}.bmp').convert('RGB'),dtype=int)
        source=np.asarray(Image.open(package/anchor['sourceImage']['path']).convert('RGB'),dtype=int)
        assert int(np.max(np.abs(image-source)))<=1,name
    for path in root.glob('prompt21-*.bmp'):
        if path.stem!='prompt21-release':assert 'Validation summary: errors=0 warnings=0' in path.with_suffix('.log').read_text(encoding='utf-8-sig')
    final=np.asarray(Image.open(root/'prompt21-final.bmp'),dtype=int);baseline=np.asarray(Image.open(root/'prompt20-final.bmp'),dtype=int)
    assert np.max(np.abs(final-baseline))==0
    output={'windows':len(list(root.glob('prompt21-*.bmp'))),'buffers':results,'finalMaxLSB':0,'nonfinite':0}
    (root/'prompt21-verification.json').write_text(json.dumps(output,indent=2),encoding='utf-8');print(json.dumps(output,indent=2))


if __name__=='__main__':verify()
