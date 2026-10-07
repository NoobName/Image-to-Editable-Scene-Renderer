"""Known diffuse+GGX plane/sphere fixtures with fine RGB texture; no learned decomposition."""
from pathlib import Path
from dataclasses import replace
import numpy as np
from PIL import Image
from specular_reference import surface,ggx
from pipeline.geometry_backend import DummyGeometryBackend
from pipeline.material_estimation_backend import NeutralMaterialBackend,linear_to_srgb,quantize
from pipeline.lighting_backend import ManualLightingBackend
from pipeline.intrinsic_backend import IntrinsicBackend,IntrinsicEstimate
from pipeline.runner import ReconstructionPipeline
from estimate_intrinsic import export_intrinsic

def create(root):
    root=Path(root);root.mkdir(exist_ok=False)
    for kind in ('plane','sphere'):
        point,normal,z,valid,k=surface(kind);h,w=z.shape;y,x=np.mgrid[:h,:w]
        a=np.broadcast_to([.2,.3,.25],point.shape).copy().astype(np.float32);a*= (.85+.15*((x+y)%2))[...,None]
        rough=np.full((h,w),.32,np.float32);metal=np.zeros((h,w),np.float32);direction=np.array([.25,-.15,1]);direction/=np.linalg.norm(direction)
        direct=np.array([.65,.6,.5]);ambient=np.array([.08,.09,.1]);shading=(np.maximum(normal@-direction,0)[...,None]*direct+ambient).astype(np.float32)
        spec=ggx(normal.astype(float),point.astype(float),a,rough,metal,direction,direct).astype(np.float32)
        rgb=quantize(linear_to_srgb(a*shading+spec));Image.fromarray(rgb).save(root/(kind+'.png'))
        class Geometry(DummyGeometryBackend):
            def predict(self,image):
                g=super().predict(image);return replace(g,normal=normal,point_map=point,depth=z,valid_mask=valid,confidence=valid.astype(np.float32),camera_intrinsics=k,
                    metadata={**g.metadata,'backend':'synthetic-GGX-'+kind})
        class Material(NeutralMaterialBackend):
            def predict(self,image):
                m=super().predict(image);return replace(m,albedo=a,roughness=rough,metallic=metal,confidence=np.ones_like(rough),
                    metadata={**m.metadata,'backend':'synthetic-GGX-ground-truth','albedo_source':'intrinsic'})
        class Intrinsic(IntrinsicBackend):
            name='synthetic-GGX'
            def predict(self,image,material=None):
                return IntrinsicEstimate(a,shading,spec,np.zeros_like(a),np.ones_like(z,dtype=np.uint32),
                    dict(backend=self.name,residual_semantics='nonnegative-non-diffuse',uncertainty_semantics='synthetic-known',gauge='known-Lambert-plus-GGX',scale_ambiguous=False,provenance='synthetic'))
        ReconstructionPipeline(geometry_backend=Geometry(),material_backend=Material(),lighting_backend=ManualLightingBackend(direction,direct,ambient)).run(root/(kind+'.png'),root/(kind+'-base'),max_size=w)
        export_intrinsic(root/(kind+'-base'),root/kind,Intrinsic())
    Image.new('L',(7,5),255).save(root/'protect-all.png')
if __name__=='__main__':create('generated/prompt25-fixtures')
