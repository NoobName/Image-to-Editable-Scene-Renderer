"""Full float evidence plus separate display-only diagnostics; never overwrites source/material."""
import numpy as np
from PIL import Image,ImageDraw
from intrinsic_contract import sha256
from runtime_dds import write_dds
from scene_package import write_json_atomic
from .material_estimation_backend import srgb_to_linear,linear_to_srgb,quantize
from .intrinsic_backend import validate_intrinsic

def write_intrinsic(root,image,appearance,result):
    validate_intrinsic(image,result);directory=root/'intrinsic';directory.mkdir(exist_ok=True)
    reconstructed=result.albedo*result.shading+(result.residual if result.residual is not None else 0)
    error=np.sqrt(np.mean((srgb_to_linear(image.rgb.astype(np.float32)/255)-reconstructed)**2,axis=-1)).astype(np.float32)
    values=dict(albedo=result.albedo,shading=result.shading,uncertainty=result.uncertainty,error=error,validity=result.validity)
    if result.residual is not None:values['residual']=result.residual
    maps={}
    for key,a in values.items():
        fmt='RGBA32_FLOAT' if a.ndim==3 else 'R32_UINT' if key=='validity' else 'R32_FLOAT'
        pixels=np.concatenate((a,np.zeros((*a.shape[:2],1),np.float32)),-1) if a.ndim==3 else a
        path=f'intrinsic/{key}.dds';write_dds(root/path,pixels,fmt);maps[key]=dict(path=path,sha256=sha256(root/path),format=fmt)
    np.savez_compressed(directory/'intrinsic.npz',**values) # authoritative floats also retained for offline reuse
    valid=result.validity!=0;meta=result.metadata
    data=dict(version=1,sourceId=appearance['sourceId'],sourceSha256=appearance['sourceImage']['sha256'],analysisSize=[image.width,image.height],colorSpace='linear-srgb',mapping='full-frame-analysis-pixel-centers',
        input=dict(path='textures/original_image.png',sha256=sha256(root/'textures/original_image.png')),residualSemantics=meta['residual_semantics'],
        gauge=dict(name=meta['gauge'],exposure=0,albedoGain=1,shadingGain=1,residualGain=1,scaleAmbiguous=meta['scale_ambiguous']),provenance=meta,
        metrics=dict(validPixels=int(valid.sum()),rmse=float(np.sqrt(np.mean(error[valid]**2))) if valid.any() else 0,maxError=float(error[valid].max()) if valid.any() else 0),maps=maps)
    write_json_atomic(directory/'intrinsic.json',data)
    previews=[('Intrinsic Albedo',quantize(linear_to_srgb(result.albedo))),('Original Diffuse Shading',quantize(linear_to_srgb(result.shading))),
        ('Non-diffuse Residual (signed gray=0)',quantize(.5+.5*result.residual) if result.residual is not None else np.zeros_like(image.rgb)),
        ('Recomposition Error x4',quantize(np.repeat(np.minimum(error*4,1)[...,None],3,axis=-1))),('Uncertainty A/S/R',quantize(result.uncertainty)),('Original (analysis)',image.rgb)]
    canvas=Image.new('RGB',(900,520),'#20252b');draw=ImageDraw.Draw(canvas)
    for i,(title,pixels) in enumerate(previews):
        tile=Image.fromarray(pixels);tile.thumbnail((292,215));x=(i%3)*300;y=(i//3)*250;canvas.paste(tile,(x,y+25));draw.text((x+3,y+3),title,fill='white')
    draw.text((4,503),f"{meta['backend']} / native scale / RMSE {data['metrics']['rmse']:.5f} / residual != specular",fill='white');canvas.save(directory/'diagnostic.png')
    return data
