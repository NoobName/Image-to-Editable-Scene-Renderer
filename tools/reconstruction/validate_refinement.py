"""Real adapter runs, replay/cache, explicit text protection and negative cases."""
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from refinement_io import digest


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--recipe',type=Path,required=True);p.add_argument('--physics',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    mask=Image.new('L',Image.open(a.physics/'original.png').size,0);draw=ImageDraw.Draw(mask)
    # Explicitly documented native-pixel ROIs for the HouseIndoor signs; not automatic OCR.
    rois=[(230,110,570,355),(940,230,1280,305)]
    for box in rois:draw.rectangle(box,fill=255)
    mask.save(a.output/'text-protection.png');cases=[]
    physics_before=digest(a.physics/'result.png')
    def run(name,extra=(),code=0):
        command=[sys.executable,str(Path(__file__).with_name('refine_image.py')),'--output',str(a.output/name),*extra]
        start=time.perf_counter()
        with (a.output/(name+'.log')).open('w',encoding='utf-8') as log:
            result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        assert result.returncode==code,(name,result.returncode)
        cases.append({'name':name,'exit':result.returncode,'seconds':time.perf_counter()-start,'command':command})
    shared=['--recipe',str(a.recipe),'--physics-export',str(a.physics),'--strength','0.2','--protect-mask',str(a.output/'text-protection.png')]
    run('real',shared)
    run('replay',['--replay',str(a.output/'real/refinement.json')])
    run('repeat-no-cache',[*shared,'--no-cache'])
    run('zero',[*shared,'--strength','0'])
    cancel=a.output/'cancel.flag';cancel.write_text('cancel',encoding='utf-8')
    run('cancel-before',[*shared,'--cancel-file',str(cancel)],2)
    assert not (a.output/'cancel-before').exists()
    run('bad-recipe',['--recipe',str(a.output/'missing.json'),'--physics-export',str(a.physics),'--strength','0.2'],2)
    original=np.asarray(Image.open(a.physics/'result.png')).astype(int)
    real=np.asarray(Image.open(a.output/'real/refined.png')).astype(int)
    replay=np.asarray(Image.open(a.output/'replay/refined.png')).astype(int)
    repeat=np.asarray(Image.open(a.output/'repeat-no-cache/refined.png')).astype(int)
    protected=np.asarray(mask)>0
    assert np.array_equal(real[protected],original[protected]),'Explicit text protection changed pixels'
    assert np.array_equal(real,replay),'Cached replay changed output'
    repeat_error=int(np.max(np.abs(real-repeat)));assert repeat_error<=1,'Fixed-seed rerun exceeds declared 1 LSB'
    assert digest(a.output/'zero/refined.png')==physics_before,'Zero strength must copy physics bytes exactly'
    assert digest(a.physics/'result.png')==physics_before,'Physics was overwritten'
    report={'cases':cases,'nativeSize':list(mask.size),'textRois':rois,'protectedTextMaxLsb':0,
        'cacheReplayMaxLsb':0,'freshRepeatMaxLsb':repeat_error,'zeroPngByteExact':True,'physicsUnchanged':True,
        'real':json.loads((a.output/'real/refinement.json').read_text(encoding='utf-8')),
        'replayCacheHit':json.loads((a.output/'replay/refinement.json').read_text(encoding='utf-8'))['cacheHit']}
    (a.output/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('cases','real')},indent=2))


if __name__=='__main__':main()
