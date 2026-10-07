"""Audit actual window/readback logs and assemble exported diagnostic previews (not new rendering)."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageOps
from verify_recipe import dds
from pipeline.material_estimation_backend import linear_to_srgb


def run(output):
    prefix=output.name
    logs=list(Path('generated').glob(prefix+'-*.log'))
    logs += [Path('generated')/(name+'.log') for name in ('prompt32-reload','prompt32-failed','prompt32-cancel','prompt32-outdoor','prompt32-painting')]
    logs += list(Path('generated').glob('prompt32-profile-*.log'))
    logs=[p for p in logs if not p.stem.endswith('-run')]
    windows=[]
    for path in logs:
        text=path.read_text(encoding='utf-8-sig')
        if 'Completed frames=' not in text:
            raise AssertionError(path)
        debug='Validation summary: errors=0 warnings=0' in text
        assert debug or 'Validation summary: disabled (Release; messages not collected)' in text,path
        windows.append(dict(log=path.as_posix(),debug=debug,warp='Microsoft Basic Render Driver' in text))
    reads=[]
    for path in Path('generated').glob(prefix+'-*/readback.json'):
        d=json.loads(path.read_text(encoding='utf-8'))
        def check(value):
            if isinstance(value,dict):
                if 'nonfinite' in value:
                    assert value['nonfinite']==0,path
                if 'byteExact' in value:
                    assert value['byteExact'],path
                if 'stateBefore' in value:
                    assert value['stateBefore']==value['stateAfter'] and value['stateCapture']==2048,path
                for child in value.values():check(child)
            elif isinstance(value,list):
                for child in value:check(child)
        check(d);reads.append(path.as_posix())
    real=output/'real-edit'
    original=np.asarray(Image.open(real/'original.png').convert('RGB'),dtype=int)
    result=np.asarray(Image.open(real/'result.png').convert('RGB'),dtype=int)
    # These are existing GPU-exported previews, arranged for inspection, never inpainted.
    canvas=Image.new('RGB',(1500,760),(20,24,30));draw=ImageDraw.Draw(canvas)
    names=[('original','Original / canonical 1500 x 1000'),('old-shading','Calculated OLD / fixed linear /4 then sRGB'),
           ('new-shading','Calculated NEW / fixed linear /4 then sRGB'),('ratio','Bounded ratio / 0.5 * RGB (1 = gray)'),
           ('confidence','Confidence / white = supported'),('result','Relighted Original RGB / native export')]
    for i,(name,title) in enumerate(names):
        x,y=(i%3)*500,(i//3)*380
        draw.text((x+12,y+12),title,fill='white')
        if name in ('old-shading','new-shading','ratio'):
            values=dds(real/(name+'.dds'))[...,:3]
            values=np.clip(values*.5,0,1) if name=='ratio' else linear_to_srgb(np.clip(values/4,0,1))
            preview=Image.fromarray(np.round(values*255).astype(np.uint8))
        else:
            preview=Image.open(real/(name+'.png')).convert('RGB')
        tile=ImageOps.contain(preview,(480,330))
        canvas.paste(tile,(x+10+(480-tile.width)//2,y+38+(330-tile.height)//2))
    canvas.save(output/'debug-overview.png')
    summary=dict(windows=windows,windowCount=len(windows),debugCount=sum(w['debug'] for w in windows),
        releaseCount=sum(not w['debug'] for w in windows),readbackReports=reads,
        realChangedPixels=int(np.any(original!=result,axis=-1).sum()),realMaxLsb=int(np.abs(original-result).max()),
        manualDesktop='not performed; CTest ImGui event injection and actual CLI HWND captures are separate')
    (output/'delivery-summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('windows','readbackReports')},indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);run(p.parse_args().output)
