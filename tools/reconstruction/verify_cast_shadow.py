"""Verify GPU transport, analytic positions, rollback and the unchanged 3D path.

Declared tolerances: float64/GPU 2e-5; analytic shadow IoU >= .85 with
no false positive beyond two analysis pixels; supported RGB restoration RMSE < .01.
Unknown/black/disabled cases must be byte-identical to the stage26 baseline.
"""
import json
from pathlib import Path
import numpy as np
from PIL import Image
from cast_shadow_reference import change
from runtime_dds import read_dds


def read(directory):
    report=json.loads((directory/'readback.json').read_text(encoding='utf-8'))
    maps={}
    for path in directory.glob('*.bin'):
        k=path.stem;entry=report[k];w,h=entry['size']
        assert entry['nonfinite']==0 and entry['stateBefore']==entry['stateAfter']==128 and entry['stateCapture']==2048,k
        maps[k]=np.fromfile(path,np.float32).reshape(h,w,-1).astype(np.float64)
        assert np.isfinite(maps[k]).all(),path
    return report,maps


def main():
    root=Path('generated');records=[]
    for directory in sorted(root.glob('prompt27-*-shading')):
        if directory.name=='prompt27-first-shading':continue
        report,maps=read(directory);name=directory.name.removesuffix('-shading')
        view=json.loads((root/(name+'.view.json')).read_text(encoding='utf-8'));package=Path(view['packageRoot'])
        c=report['composition']['castShadow'];error=0.
        if c['available']:
            anchor=json.loads((package/'relighting/relighting.json').read_text(encoding='utf-8'))
            encoded=np.asarray(Image.open(package/anchor['sourceImage']['path']).convert('RGB'),np.float64)/255
            _,normal=read_dds(package/'analysis/normal.dds');normal=normal[...,:3]
            # Fixed normalized source mapping: always sample analysis at source pixel centers.
            h,w=encoded.shape[:2];ah,aw=normal.shape[:2];y=np.minimum(((np.arange(h)+.5)*ah/h).astype(int),ah-1);x=np.minimum(((np.arange(w)+.5)*aw/w).astype(int),aw-1)
            sample=lambda v:v[y[:,None],x[None,:]]
            composition=report['composition']
            delta,weight=change(encoded,sample(normal),sample(maps['castOldVisibility']),sample(maps['castNewVisibility']),sample(maps['castEvidence']),sample(maps['castResidual']),report['source'],report['target'],
                epsilon=composition['epsilon'],strength=composition['strength']*c['strength'],confidence=c['confidenceScale'],max_delta=c['maxDeltaPerSource'],enabled=c['enabled'])
            expected=np.maximum(maps['baseline26'][...,:3]+delta,0)
            error=float(abs(expected-maps['result'][...,:3]).max());assert error<2e-5,(name,error)
            inactive=weight==0;assert np.array_equal(maps['result'][inactive],maps['baseline26'][inactive]),name
        else:
            assert np.array_equal(maps['result'],maps['baseline26']),name
        records.append(dict(name=name,cpuGpuMax=error,changedPixels=int(np.any(maps['result']!=maps['baseline26'],axis=-1).sum()),
            maxChange=float(abs(maps['result']-maps['baseline26']).max()),oldUpdates=c.get('oldUpdates',0),newUpdates=c.get('newUpdates',0)))
    prefix=root/'prompt27-cast-final-shading';report,maps=read(prefix);truth=np.load(root/'prompt27-final-fixtures/plane-truth.npz');analytic=[]
    for kind,key in [('old','castOldVisibility'),('new','castNewVisibility')]:
        expected=truth[kind];actual=maps[key][...,0]<.5;intersection=int((actual&expected).sum());union=int((actual|expected).sum())
        padded=np.pad(expected,2);near=np.zeros_like(expected)
        for y in range(5):
            for x in range(5):near|=padded[y:y+expected.shape[0],x:x+expected.shape[1]]
        outside=int((actual&~near).sum());iou=intersection/union
        assert iou>=.85 and outside==0,(kind,iou,outside)
        centroidError=float(np.linalg.norm(np.argwhere(actual).mean(0)-np.argwhere(expected).mean(0)))
        analytic.append(dict(kind=kind,referencePixels=int(expected.sum()),predictedPixels=int(actual.sum()),intersection=intersection,iou=iou,falseOutsideTwoPixels=outside,centroidErrorPixels=centroidError))
    supported=(maps['castEvidence'][...,1]>.5)&(maps['castOldVisibility'][...,0]<.5)
    rmse=float(np.sqrt(np.mean((maps['result'][supported,:3]-truth['target'][supported])**2)))
    assert rmse<.01,rmse
    targetSupport=maps['castNewVisibility'][...,0]<.5
    ambientError=float(np.sqrt(np.mean((maps['result'][targetSupport,:3]-truth['albedo'][targetSupport]*.4)**2)))
    assert ambientError<.01,ambientError
    for name in ('identity','shadow-off','shadow-zero','shadow-confidence-zero','shadow-reset','shadow-direct-off','missing','emission','black','no-confidence','lighting-source'):
        d=root/('prompt27-'+name+'-shading');assert (d/'result.bin').read_bytes()==(d/'baseline26.bin').read_bytes(),name
    for key in ('castOldMap','castOldVisibility'):
        assert (prefix/(key+'.bin')).read_bytes()==(root/'prompt27-identity-shading'/(key+'.bin')).read_bytes(),key
    for key in ('castOldMap','castNewMap','castOldVisibility','castNewVisibility','result'):
        assert (prefix/(key+'.bin')).read_bytes()==(root/'prompt27-shadow-scene-edit-shading'/(key+'.bin')).read_bytes(),key
    assert report['composition']['castShadow']['oldUpdates']==1 and report['composition']['castShadow']['newUpdates']==2
    # Asymmetric Y is a separate fixture, preventing a vertically mirrored implementation from passing.
    _,asym=read(root/'prompt27-asymmetric-shading');asymTruth=np.load(root/'prompt27-final-fixtures/asymmetric-truth.npz')
    for kind in ('old','new'):
        actual=asym['cast'+kind.title()+'Visibility'][...,0]<.5;expected=asymTruth[kind]
        assert (actual&expected).sum()/(actual|expected).sum()>.85
    assert (root/'prompt27-3d.bmp').read_bytes()==(root/'prompt26-final-after.bmp').read_bytes(),'3D shadow regression'
    assert (root/'prompt27-source-native.bmp').read_bytes()==(root/'prompt26-source-after.bmp').read_bytes(),'Canonical Source regression'
    assert (root/'prompt27-unavailable-shading/result.bin').read_bytes()==(root/'prompt26-before-shading/result.bin').read_bytes(),'Legacy image regression'
    transaction=(root/'prompt27-transaction.log').read_text(encoding='utf-8-sig');assert 'failed GPU preparation retained' in transaction and 'cancelled completed upload retained' in transaction
    assert (root/'prompt27-reload.log').read_text(encoding='utf-8-sig').count('Reconstruction Ready:')==2
    realReport,real=read(root/'prompt27-real-shading');realPackage=Path('generated/scene26-final-indoor');_,labels=read_dds(realPackage/'analysis/region.dds')
    delta=real['result'][...,:3]-real['baseline26'][...,:3];luma=delta@np.array([.2126,.7152,.0722]);regions=[]
    scene=json.loads((realPackage/'scene.json').read_text(encoding='utf-8'))
    for obj in scene['objects']:
        if 'region' not in obj:continue
        r=json.loads((realPackage/obj['region']).read_text(encoding='utf-8'));mask=labels==r['labelId']
        regions.append(dict(name=r['name'],pixels=int(mask.sum()),brightened=int((luma[mask]>1e-6).sum()),darkened=int((luma[mask]<-1e-6).sum()),
            unsupportedReceiver=int((real['castOldVisibility'][mask,1]==0).sum()),oldPhotoSupported=int((real['castEvidence'][mask,1]>0).sum())))
    windows=[]
    for bmp in sorted(root.glob('prompt27-*.bmp')):
        if bmp.stem=='prompt27-first':continue
        log=bmp.with_suffix('.log').read_text(encoding='utf-8-sig')
        if bmp.stem!='prompt27-release':assert 'Validation summary: errors=0 warnings=0' in log,bmp
        windows.append(bmp.stem)
    output=dict(tolerances=dict(floatMax=2e-5,shadowIoU=.85,falseOutsidePixels=2,restorationRMSE=.01),readbacks=records,analytic=analytic,
        oldRestorationRMSE=rmse,newShadowAmbientRMSE=ambientError,identityAndDisabledByteExact=True,immutableSource=True,oldCachePreserved=True,
        legacy3DByteExact=True,legacyImageByteExact=True,realRegions=regions,realGeometry=realReport['composition']['castShadow'],windows=windows)
    (root/'prompt27-verification.json').write_text(json.dumps(output,indent=2),encoding='utf-8');print(json.dumps(output,indent=2))


if __name__=='__main__':main()
