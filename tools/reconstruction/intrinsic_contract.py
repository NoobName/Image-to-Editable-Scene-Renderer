"""Strict optional intrinsic sidecar: fixed RGB observation, native float outputs, explicit gauge."""
from pathlib import Path
import numpy as np
from PIL import Image
try:
    from .package_schema import read_json,validate,_check_profile
    from .runtime_dds import read_dds
    from .lighting_contract import sha256
except ImportError:
    from package_schema import read_json,validate,_check_profile
    from runtime_dds import read_dds
    from lighting_contract import sha256
KEYS=('albedo','shading','residual','uncertainty','error','validity')
SCHEMA=read_json(Path(__file__).resolve().parents[2]/'schemas/intrinsic.schema.json');_check_profile(SCHEMA)

def read_arrays(root,data):
    return {k:read_dds(Path(root)/r['path'])[1] for k,r in data['maps'].items()}

def load_intrinsic(root,appearance):
    root=Path(root)
    if not (root/'intrinsic/intrinsic.json').exists():return None
    try:
        try:from .scene_package import asset_path
        except ImportError:from scene_package import asset_path
        data=read_json(asset_path(root,'intrinsic/intrinsic.json','intrinsic',('.json',)));validate(data,SCHEMA,schema=SCHEMA)
        if appearance is None or data['sourceId']!=appearance['sourceId'] or data['sourceSha256']!=appearance['sourceImage']['sha256'] or data['analysisSize']!=appearance['analysisImage']['size']:raise ValueError('Intrinsic source identity/size mismatch')
        if data['input']['path']!='textures/original_image.png':raise ValueError('Intrinsic fixed observation path mismatch')
        w,h=data['analysisSize'];arrays={}
        for key,r in [('input',data['input']),*data['maps'].items()]:
            path=asset_path(root,r['path'],'textures' if key=='input' else 'intrinsic',('.png',) if key=='input' else ('.dds',))
            if path.stat().st_size>128*1024*1024 or len(r['sha256'])!=64 or any(c not in '0123456789abcdef' for c in r['sha256']) or sha256(path)!=r['sha256']:raise ValueError(f'Intrinsic hash/budget mismatch: {key}')
            if key=='input':continue
            fmt,a=read_dds(path);expected='R32_UINT' if key=='validity' else 'R32_FLOAT' if key=='error' else 'RGBA32_FLOAT'
            if fmt!=expected or r['format']!=expected or a.shape[:2]!=(h,w):raise ValueError(f'Intrinsic map format/size mismatch: {key}')
            lo=-64 if key=='residual' and data['residualSemantics']=='signed-non-diffuse' else 0;hi=1 if key in ('albedo','validity') else 128 if key=='error' else 64
            if np.any(a<lo) or np.any(a>hi) or a.ndim==3 and np.any(a[...,3]!=0):raise ValueError(f'Intrinsic values/range invalid: {key}')
            arrays[key]=a
        p=data['provenance']
        if ('residual' in arrays)!=(data['residualSemantics']!='unavailable') or data['residualSemantics']!=p['residual_semantics'] or data['gauge']['name']!=p['gauge'] or data['gauge']['scaleAmbiguous']!=p['scale_ambiguous']:raise ValueError('Intrinsic semantics/gauge mismatch')
        from pipeline.material_estimation_backend import srgb_to_linear
        original=np.asarray(Image.open(root/data['input']['path']).convert('RGB'),dtype=np.float32)/255
        if original.shape!=(h,w,3):raise ValueError('Intrinsic observation size mismatch')
        reconstructed=arrays['albedo'][...,:3]*arrays['shading'][...,:3]+(arrays['residual'][...,:3] if 'residual' in arrays else 0)
        error=np.sqrt(np.mean((srgb_to_linear(original)-reconstructed)**2,axis=-1));mask=arrays['validity']!=0
        if not np.allclose(error,arrays['error'],rtol=1e-5,atol=1e-5):raise ValueError('Intrinsic recomposition error mismatch')
        metrics=data['metrics'];rmse=float(np.sqrt(np.mean(error[mask]**2))) if mask.any() else 0;maximum=float(error[mask].max()) if mask.any() else 0
        if metrics['validPixels']!=int(mask.sum()) or abs(metrics['rmse']-rmse)>1e-5 or abs(metrics['maxError']-maximum)>1e-5:raise ValueError('Intrinsic statistics mismatch')
        return data
    except (ValueError,KeyError,OSError) as error:raise ValueError(f'Invalid optional intrinsic/intrinsic.json: {error}') from error
