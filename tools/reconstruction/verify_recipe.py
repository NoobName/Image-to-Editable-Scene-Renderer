"""Stage29 independent image/float/metadata checks; never modify source packages."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image


def dds(path):
    data=path.read_bytes();header=np.frombuffer(data[:148],dtype='<u4')
    assert header[0]==0x20534444 and header[1]==124 and header[21]==0x30315844
    w,h=int(header[4]),int(header[3]);channels=4 if header[32]==2 else 1
    assert header[32] in (2,41,42) and w<=8192 and h<=8192 and w*h<=8*1024*1024
    values=np.frombuffer(data[148:],dtype='<u4' if header[32]==42 else '<f4').reshape(h,w,channels)
    assert np.isfinite(values).all()
    return values


def verify(root):
    root=Path(root);report={'tolerances':{'nativeRgbLsb':1,'roundTripLsb':0,'roundTripFloat':0,'warpFloat':2e-5},'comparisons':{}}
    original=np.asarray(Image.open(root/'native/original.png')).astype(int)
    native=np.asarray(Image.open(root/'native/result.png')).astype(int)
    assert native.shape==(1000,1500,3)
    error=int(np.max(np.abs(native-original)));assert error<=1;report['nativeMaxLsb']=error
    for base,other in [('edit','reopen'),('edit','ui'),('edit','release'),('edit','reload'),('edit','cycle'),('edit','moved'),('edit','bad-hash-retained'),('native','cancel'),('native','missing'),('calibrated','calibrated-reopen'),('real','real-reopen')]:
        a=np.asarray(Image.open(root/base/'result.png')).astype(int);b=np.asarray(Image.open(root/other/'result.png')).astype(int)
        error=int(np.max(np.abs(a-b)));assert error==0,(base,other,error)
        for name in ('result','old-shading','new-shading','ratio','confidence'):
            assert np.array_equal(dds(root/base/(name+'.dds')),dds(root/other/(name+'.dds'))),(base,other,name)
        report['comparisons'][base+'->'+other]={'maxLsb':error,'floatExact':True}
    error=float(np.max(np.abs(dds(root/'edit/result.dds')-dds(root/'warp/result.dds'))));assert error<=2e-5
    report['warpMaxFloat']=error
    if (root/'mask-reload').exists():
        for name in ('result','old-shading','new-shading','ratio','confidence'):
            assert np.array_equal(dds(root/'mask'/(name+'.dds')),dds(root/'mask-reload'/(name+'.dds')))
        report['importedMaskReloadExact']=True
    if (root/'reload-final').exists():
        assert np.array_equal(dds(root/'edit/result.dds'),dds(root/'reload-final/result.dds'))
        capture=json.loads(Path('generated/prompt29-reload-final-shading/readback.json').read_text(encoding='utf-8'))
        assert capture['result']['size']==[129,97]
        report['captureAfterRecipeCommit']=True
    for path in root.glob('*/export.json'):
        data=json.loads(path.read_text(encoding='utf-8'))
        for buffer in data['buffers'].values():
            assert buffer['nonfinite']==0 and buffer['before']==buffer['after']==128 and buffer['capture']==2048
        for path in path.parent.glob('*.dds'):dds(path)
    (root/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('root');verify(parser.parse_args().root)
