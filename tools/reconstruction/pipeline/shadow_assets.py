"""Analysis-only sidecar and layered manual evidence; no RGB asset is modified."""
from pathlib import Path
import shutil
import numpy as np
from PIL import Image,ImageDraw
from runtime_dds import write_dds
from lighting_contract import sha256
from shadow_contract import KEYS,INPUTS
from scene_package import write_json_atomic
from package_schema import read_json

def invalidate_shadow(private_root):
    root=Path(private_root).resolve();path=(root/'shadow').resolve()
    if not path.is_relative_to(root):raise ValueError('Shadow cache escaped private staging')
    if path.exists():shutil.rmtree(path)

def write_shadow(root,appearance,maps,parameters,diagnostics,confirm=None,protect=None):
    directory=root/'shadow';directory.mkdir();h,w=maps['candidate'].shape;manual={}
    for key,source in [('confirm',confirm),('protect',protect)]:
        value=np.zeros((h,w),np.float32)
        if source is not None:
            path=Path(source)
            if path.stat().st_size>16*1024*1024:raise ValueError('Shadow manual PNG exceeds 16 MiB')
            with Image.open(path) as im:
                if im.format!='PNG' or max(im.size)>2048 or im.width*im.height>2048**2:raise ValueError('Shadow mask must be a bounded PNG')
                a=np.asarray(im.convert('RGBA'))
            if np.any(a[...,0]!=a[...,1]) or np.any(a[...,0]!=a[...,2]) or np.any(a[...,3]!=255):raise ValueError('Shadow mask requires opaque gray')
            mh,mw=a.shape[:2];value=(a[((np.arange(h)+.5)*mh/h).astype(int)[:,None],((np.arange(w)+.5)*mw/w).astype(int),0]/255).astype(np.float32)
            relative=f'shadow/manual_{key}.png';shutil.copyfile(path,root/relative)
            manual[key]=dict(path=relative,sha256=sha256(root/relative),size=[mw,mh],mapping='normalized-source-nearest')
        maps['manualConfirm' if key=='confirm' else 'manualProtect']=value
    maps['effectiveCandidate']=np.maximum(maps['candidate'],maps['manualConfirm'])*(1-maps['manualProtect'])
    records={}
    for key in KEYS:
        fmt='R32_UINT' if key=='unknown' else 'R32_FLOAT';relative=f'shadow/{key}.dds';write_dds(root/relative,maps[key],fmt)
        records[key]=dict(path=relative,sha256=sha256(root/relative),format=fmt)
    np.savez_compressed(directory/'shadow.npz',**maps)
    data=dict(version=1,sourceId=appearance['sourceId'],sourceSha256=appearance['sourceImage']['sha256'],analysisSize=[w,h],
        space='source-aligned-analysis-pixel-centers',units='unitless-support-not-ground-truth',inputs=[dict(path=p,sha256=sha256(root/p)) for p in INPUTS],
        regionInputs=[dict(path=p,sha256=sha256(root/p)) for p in sorted({obj['region'] for obj in read_json(root/'scene.json')['objects'] if 'region' in obj})],
        parameters=parameters,diagnostics=diagnostics,manual=manual,maps=records)
    write_json_atomic(directory/'shadow.json',data)
    canvas=Image.new('RGB',(900,500),'#20252b');draw=ImageDraw.Draw(canvas)
    for i,key in enumerate(('candidate','visibility','geometrySupport','confidence','unknown','effectiveCandidate')):
        tile=Image.fromarray(np.rint(maps[key]*255).astype(np.uint8));tile.thumbnail((292,213));x=i%3*300;y=i//3*250
        canvas.paste(tile,(x,y+25));draw.text((x+4,y+4),key,fill='white')
    canvas.save(directory/'diagnostic.png');return data
