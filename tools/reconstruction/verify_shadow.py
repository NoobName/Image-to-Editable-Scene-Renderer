"""Numerical evidence for diagnostics and bitwise preservation of the stage-25 renderer."""
import json
from pathlib import Path
import numpy as np
from PIL import Image
from shadow_examples import fixture
from pipeline.shadow_backend import DepthShellShadowBackend
from runtime_dds import read_dds
from shadow_contract import KEYS,load_shadow
from appearance_contract import load_extension

def main():
    root=Path('generated');cpu=[]
    for name,kwargs in [('occluder',{}),('missing',{'missing':True}),('offscreen',{'offscreen':True})]:
        o,truth,dark=fixture(**kwargs);maps,_=DepthShellShadowBackend().predict(o);detected=maps['candidate']>0
        recall=float((detected&truth).sum()/truth.sum());false=int(detected[~truth&dark].sum())
        assert false==0
        if name=='occluder':assert recall>.7
        else:assert detected.sum()==0 and maps['unknown'].all()
        cpu.append(dict(case=name,truthPixels=int(truth.sum()),candidates=int(detected.sum()),recall=recall,darkTextureFalsePositives=false,unknownPixels=int(maps['unknown'].sum()),visibilityError=float(abs(maps['visibility'][truth]-.35).max())))
    sets=0
    for directory in sorted(root.glob('prompt26-*-shadow')):
        if directory.name=='prompt26-first-shadow':continue # Historical preliminary contract, retained separately.
        report=json.loads((directory/'readback.json').read_text(encoding='utf-8'));view=json.loads(directory.with_name(directory.name.removesuffix('-shadow')+'.view.json').read_text(encoding='utf-8'))
        package=Path(view['packageRoot']);data=load_shadow(package,load_extension(package));assert data
        for k in KEYS:
            entry=report[k];assert entry['byteExact'] and entry['nonfinite']==0 and entry['stateBefore']==entry['stateAfter']==128 and entry['stateCapture']==2048
            _,pixels=read_dds(package/data['maps'][k]['path']);assert pixels.tobytes()==(directory/(k+'.bin')).read_bytes()
        sets+=1
    # Existing assets are byte-preserved, including the canonical anchor and lighting calibration.
    before=root/'scene24-final-indoor';after=root/'scene26-final-indoor'
    for p in before.rglob('*'):
        if p.is_file():assert p.read_bytes()==(after/p.relative_to(before)).read_bytes(),p
    for k in ('old','new','ratio','result','qualityA','qualityB','specularOld','specularNew','specularCandidate','specularProtected'):
        assert (root/f'prompt26-before-shading/{k}.bin').read_bytes()==(root/f'prompt26-after-shading/{k}.bin').read_bytes(),k
    for a,b in [('prompt26-before.bmp','prompt26-after.bmp'),('prompt26-source-before.bmp','prompt26-source-after.bmp'),('prompt26-final-before.bmp','prompt26-final-after.bmp')]:
        assert (root/a).read_bytes()==(root/b).read_bytes(),(a,b)
    previewMax=0
    for index,k in enumerate(KEYS):
        view=('shadow-candidate','shadow-visibility','shadow-geometry','shadow-confidence','shadow-unknown','shadow-manual-confirm','shadow-manual-protect','shadow-effective')[index]
        pixels=np.asarray(Image.open(root/f'prompt26-{view}.bmp').convert('RGB'),dtype=int)
        _,values=read_dds(root/f'prompt26-final-fixtures/occluder/shadow/{k}.dds')
        # Win32 minimum caption width can expand a 129-pixel request to 152. Use the recorded
        # image rectangle and pixel centers, not the requested command-line window size.
        metadata=json.loads((root/f'prompt26-{view}.view.json').read_text(encoding='utf-8'));rx,ry,rw,rh=metadata['imageRect']
        yy,xx=np.mgrid[:pixels.shape[0],:pixels.shape[1]];u=(xx+.5-rx)/rw;v=(yy+.5-ry)/rh;inside=(u>=0)&(u<1)&(v>=0)&(v<1)
        qx=np.clip(np.floor(u*values.shape[1]).astype(int),0,values.shape[1]-1);qy=np.clip(np.floor(v*values.shape[0]).astype(int),0,values.shape[0]-1)
        expected=np.rint(values[qy,qx]*255).astype(int);error=int(abs(pixels[inside]-expected[inside,None]).max());assert error<=1,(view,error);previewMax=max(previewMax,error)
    changed=json.loads((root/'prompt26-lighting-source.view.json').read_text(encoding='utf-8'));assert not changed['shadow']['cacheValid']
    for k in KEYS:assert (root/f'prompt26-input-shadow/{k}.bin').read_bytes()==(root/f'prompt26-cycle-shadow/{k}.bin').read_bytes()
    transaction=(root/'prompt26-transaction.log').read_text(encoding='utf-8-sig');assert 'injected malformed shadow upload candidate' in transaction and 'failed GPU preparation retained' in transaction and 'cancelled completed upload retained' in transaction
    reload=(root/'prompt26-reload.log').read_text(encoding='utf-8-sig');assert reload.count('Reconstruction Ready:')==2
    gpu=[]
    for bmp in sorted(root.glob('prompt26-*.bmp')):
        log=bmp.with_suffix('.log').read_text(encoding='utf-8-sig')
        if bmp.stem!='prompt26-release':assert 'Validation summary: errors=0 warnings=0' in log,bmp
        gpu.append(bmp.stem)
    real=json.loads((after/'shadow/shadow.json').read_text(encoding='utf-8'))
    _,labels=read_dds(after/'analysis/region.dds');_,candidate=read_dds(after/'shadow/candidate.dds');_,unknown=read_dds(after/'shadow/unknown.dds');regions=[]
    for obj in json.loads((after/'scene.json').read_text(encoding='utf-8'))['objects']:
        if 'region' not in obj:continue
        r=json.loads((after/obj['region']).read_text(encoding='utf-8'));mask=labels==r['labelId']
        regions.append(dict(name=r['name'],pixels=int(mask.sum()),candidatePixels=int((candidate[mask]>0).sum()),unknownPixels=int(unknown[mask].sum())))
    output=dict(cpu=cpu,readbackSets=sets,previewMaxLSB=previewMax,unchangedFinal=True,unchangedSource=True,unchangedTenCompositeBuffers=True,sourceAssetsByteExact=True,
                real=real['diagnostics'],realRegions=regions,gpuWindows=gpu)
    (root/'prompt26-verification.json').write_text(json.dumps(output,indent=2),encoding='utf-8');print(json.dumps(output,indent=2))
if __name__=='__main__':main()
