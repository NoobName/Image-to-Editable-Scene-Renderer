"""Optional strict analysis cache; fingerprints source camera, geometry, intrinsics and light."""
from pathlib import Path
import hashlib
import numpy as np
try:
    from .package_schema import read_json,validate,_check_profile
    from .runtime_dds import read_dds,header
    from .lighting_contract import sha256
except ImportError:
    from package_schema import read_json,validate,_check_profile
    from runtime_dds import read_dds,header
    from lighting_contract import sha256
KEYS=('candidate','visibility','geometrySupport','confidence','unknown','manualConfirm','manualProtect','effectiveCandidate')
INPUTS=('relighting/relighting.json','analysis/analysis.json','intrinsic/intrinsic.json','lighting/lighting.json',
        'analysis/depth.dds','analysis/normal.dds','analysis/position.dds','analysis/validity.dds','analysis/region.dds','analysis/roughness.dds','analysis/metallic.dds')
SCHEMA=read_json(Path(__file__).resolve().parents[2]/'schemas/shadow.schema.json');_check_profile(SCHEMA)

def load_shadow(root,appearance):
    root=Path(root)
    if not (root/'shadow/shadow.json').exists():return None
    try:
        from .scene_package import asset_path
    except ImportError:
        from scene_package import asset_path
    try:
        data=read_json(asset_path(root,'shadow/shadow.json','shadow',('.json',)));validate(data,SCHEMA,schema=SCHEMA)
        if appearance is None or data['sourceId']!=appearance['sourceId'] or data['sourceSha256']!=appearance['sourceImage']['sha256'] or data['analysisSize']!=appearance['analysisImage']['size']:raise ValueError('shadow source identity/dimensions mismatch')
        if [r['path'] for r in data['inputs']]!=list(INPUTS):raise ValueError('shadow input list mismatch')
        scene=read_json(root/'scene.json')
        regions=sorted({obj['region'] for obj in scene['objects'] if 'region' in obj})
        if [r['path'] for r in data['regionInputs']]!=regions:raise ValueError('shadow region source list mismatch')
        for r in data['regionInputs']:asset_path(root,r['path'],'objects',('.json',))
        for r in (*data['inputs'],*data['regionInputs'],*data['maps'].values(),*data['manual'].values()):
            path=asset_path(root,r['path'],r['path'].split('/')[0],('.json','.dds','.png'))
            if path.stat().st_size>128*1024*1024 or len(r['sha256'])!=64 or sha256(path)!=r['sha256']:raise ValueError('shadow stale dependency/hash: '+r['path'])
        arrays={};w,h=data['analysisSize']
        for i,k in enumerate(KEYS):
            record=data['maps'][k];fmt,a=read_dds(asset_path(root,record['path'],'shadow',('.dds',)));expected='R32_UINT' if k=='unknown' else 'R32_FLOAT'
            if fmt!=expected or record['format']!=expected or a.shape!=(h,w) or np.any((a<0)|(a>1)):raise ValueError('shadow map range/format/size mismatch')
            if hashlib.sha256(header(w,h,fmt)+a.tobytes()).hexdigest()!=record['sha256']:raise ValueError('shadow numeric snapshot changed during validation')
            arrays[k]=a
        from PIL import Image
        for k,r in data['manual'].items():
            path=asset_path(root,r['path'],'shadow',('.png',))
            if path.stat().st_size>16*1024*1024:raise ValueError('shadow manual mask exceeds 16 MiB')
            with Image.open(path) as im:
                if im.format!='PNG' or list(im.size)!=r['size'] or max(im.size)>2048 or im.width*im.height>2048**2:raise ValueError('shadow manual mask dimensions/format')
                a=np.asarray(im.convert('RGBA'))
            if np.any(a[...,0]!=a[...,1]) or np.any(a[...,0]!=a[...,2]) or np.any(a[...,3]!=255):raise ValueError('shadow mask requires opaque grayscale')
            mh,mw=a.shape[:2];sample=a[((np.arange(h)+.5)*mh/h).astype(int)[:,None],((np.arange(w)+.5)*mw/w).astype(int),0]/255
            if not np.allclose(arrays['manualConfirm' if k=='confirm' else 'manualProtect'],sample,atol=1e-7,rtol=0):raise ValueError('shadow manual layer mismatch')
        for key,mapkey in [('confirm','manualConfirm'),('protect','manualProtect')]:
            if key not in data['manual'] and np.any(arrays[mapkey]!=0):raise ValueError('shadow manual data without provenance')
        effective=np.maximum(arrays['candidate'],arrays['manualConfirm'])*(1-arrays['manualProtect'])
        if not np.allclose(effective,arrays['effectiveCandidate'],atol=1e-6,rtol=0):raise ValueError('shadow effective layer mismatch')
        if np.any(arrays['candidate'][arrays['unknown']!=0]!=0) or np.any((arrays['candidate']>0)&(arrays['geometrySupport']==0)):raise ValueError('shadow candidate without evidence')
        if data['diagnostics']['candidatePixels']!=int((arrays['candidate']>0).sum()) or data['diagnostics']['unknownPixels']!=int(arrays['unknown'].sum()):raise ValueError('shadow statistics mismatch')
        return data
    except (ValueError,OSError,KeyError) as e:raise ValueError(f'Invalid optional shadow/shadow.json: {e}') from e
