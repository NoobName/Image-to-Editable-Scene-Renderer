"""Known cross-region occluder, textured receiver and deliberately missing blocker."""
from pathlib import Path
from dataclasses import replace
import numpy as np
from PIL import Image
from pipeline.shadow_backend import ShadowInput,DepthShellShadowBackend
from pipeline.lighting_backend import light_record

def fixture(missing=False,offscreen=False,center_y=0.):
    w,h=129,97;y,x=np.mgrid[:h,:w];fy=.5/np.tan(np.radians(60)/2);fx=fy*h/w
    k=np.array([[fx,0,.5],[0,fy,.5],[0,0,1]],np.float32)
    rays=np.stack((((x+.5)/w-.5)/fx,-((y+.5)/h-.5)/fy,np.ones_like(x)),axis=-1).astype(np.float32)
    center,half=(1.7,.1) if offscreen else (0,.25)
    foreground=(abs(rays[...,0]*1.8-center)<half)&(abs(rays[...,1]*1.8-center_y)<.35)
    depth=np.where(foreground&(not missing),1.8,3).astype(np.float32);points=rays*depth[...,None]
    truth=(abs(points[...,0]+.9-center)<half)&(abs(points[...,1]-center_y)<.35)&(depth>2)
    normal=np.zeros_like(points);normal[...,2]=-1
    labels=np.where(foreground&(not missing),2,1).astype(np.uint32)
    a=np.broadcast_to([.4,.3,.2],points.shape).copy().astype(np.float32)
    dark=((x//9+y//7)%2==0);a[dark]*=.2
    d=np.array([-.6,0,.8]);direct=np.array([.7,.6,.5]);ambient=np.array([.1,.1,.1])
    visibility=np.where(truth,.35,1).astype(np.float32)
    shading=(.8*visibility[...,None]*direct+ambient).astype(np.float32)
    z=np.zeros((h,w),np.float32)
    o=ShadowInput(depth,normal,points,k,np.ones_like(z,bool),labels,np.full_like(z,.65),z,a,shading,z,z,np.zeros_like(z,bool),
        light_record(-d,direct,ambient,'manual'),1.,1.)
    return o,truth,dark

def create(root):
    from pipeline.geometry_backend import DummyGeometryBackend
    from pipeline.material_estimation_backend import NeutralMaterialBackend,linear_to_srgb,quantize
    from pipeline.segmentation_backend import SegmentationBackend
    from pipeline.segmentation_types import SegmentationPrediction,MaskProposal
    from pipeline.lighting_backend import ManualLightingBackend,coefficients
    from pipeline.intrinsic_backend import IntrinsicBackend,IntrinsicEstimate
    from pipeline.runner import ReconstructionPipeline
    from estimate_intrinsic import export_intrinsic
    from estimate_shadows import export_shadows
    root=Path(root);root.mkdir(exist_ok=False)
    for kind in ('occluder','missing'):
        o,truth,dark=fixture(kind=='missing');h,w=o.depth.shape
        Image.fromarray(quantize(linear_to_srgb(o.albedo*o.shading))).save(root/(kind+'.png'))
        class Geometry(DummyGeometryBackend):
            def predict(self,image):
                g=super().predict(image);return replace(g,normal=o.normal,point_map=o.points,depth=o.depth,valid_mask=o.valid,confidence=o.valid.astype(np.float32),camera_intrinsics=o.intrinsics,
                    metadata={**g.metadata,'backend':'synthetic-shadow-'+kind})
        class Material(NeutralMaterialBackend):
            def predict(self,image):
                m=super().predict(image);return replace(m,albedo=o.albedo,roughness=o.roughness,metallic=o.metallic,confidence=np.ones_like(o.depth),
                    metadata={**m.metadata,'backend':'synthetic-shadow-ground-truth','albedo_source':'intrinsic'})
        class Segmentation(SegmentationBackend):
            name='synthetic-shadow-regions'
            def predict(self,image):
                return SegmentationPrediction(tuple(MaskProposal(o.labels==i,object_id=f'object-{i}',name='Receiver' if i==1 else 'Blocker',category='ground' if i==1 else 'major') for i in np.unique(o.labels)))
        class Intrinsic(IntrinsicBackend):
            name='synthetic-shadow'
            def predict(self,image,material=None):
                return IntrinsicEstimate(o.albedo,o.shading,np.zeros_like(o.albedo),np.zeros_like(o.albedo),o.valid.astype(np.uint32),
                    dict(backend=self.name,residual_semantics='nonnegative-non-diffuse',uncertainty_semantics='synthetic-known',gauge='known-shadow-Lambert',scale_ambiguous=False,provenance='synthetic'))
        direct,ambient=coefficients(o.light)
        ReconstructionPipeline(geometry_backend=Geometry(),segmentation_backend=Segmentation(),material_backend=Material(),lighting_backend=ManualLightingBackend(o.light['direction'],direct,ambient)).run(root/(kind+'.png'),root/(kind+'-base'),max_size=w)
        export_intrinsic(root/(kind+'-base'),root/(kind+'-intrinsic'),Intrinsic())
        export_shadows(root/(kind+'-intrinsic'),root/kind,shading_scale=1)
        np.savez_compressed(root/(kind+'-truth.npz'),shadow=truth,dark=dark)
    Image.new('L',(7,5),255).save(root/'protect.png')
    mask=np.zeros((5,7),np.uint8);mask[0,0]=255;Image.fromarray(mask).save(root/'confirm.png')
    export_shadows(root/'occluder',root/'manual',root/'confirm.png',root/'protect.png',shading_scale=1)
if __name__=='__main__':create('generated/prompt26-final-fixtures')
