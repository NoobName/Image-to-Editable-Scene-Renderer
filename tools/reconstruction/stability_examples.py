"""Fixed 195x99 source / 65x33 analysis with normal/depth/region edges and stress tiles."""
import argparse
from dataclasses import replace
from pathlib import Path
import numpy as np
from PIL import Image
from pipeline.runner import ReconstructionPipeline
from pipeline.geometry_backend import DummyGeometryBackend,pinhole_points
from pipeline.segmentation_backend import SegmentationBackend
from pipeline.segmentation_types import MaskProposal,SegmentationPrediction
from pipeline.material_estimation_backend import NeutralMaterialBackend,srgb_to_linear
from pipeline.lighting_backend import ManualLightingBackend


class Geometry(DummyGeometryBackend):
    name='stability-test-geometry'

    def predict(self,image):
        p=super().predict(image);h,w=p.depth.shape;x=np.arange(w)[None,:]
        depth=np.where(np.broadcast_to(x<w//2,(h,w)),3.,7.).astype(np.float32)
        normal=p.normal.copy();normal[:,w//2:,0]=.6;normal[:,w//2:,2]=-.8
        valid=p.valid_mask.copy();valid[3:8,47:53]=False
        depth[~valid]=0;normal[~valid]=0;points=pinhole_points(depth,p.camera_intrinsics);points[~valid]=0
        return replace(p,depth=depth,normal=normal,point_map=points,valid_mask=valid,confidence=valid.astype(np.float32))


class Regions(SegmentationBackend):
    name='stability-test-regions'

    def predict(self,image):
        h,w=image.height,image.width;x=np.broadcast_to(np.arange(w)[None,:],(h,w))
        return SegmentationPrediction((MaskProposal(x<w//2,1,'left','Left diffuse','major'),
            MaskProposal(x>=w//2,.6,'right','Right diffuse','background')),{'backend':self.name})


class Materials(NeutralMaterialBackend):
    name='stability-test-materials'

    def predict(self,image):
        p=super().predict(image);rgb=srgb_to_linear(image.rgb.astype(np.float32)/255)
        h,w=image.height,image.width;response=np.ones((h,w,1),np.float32)*1.2;response[:,w//2:]=1.0
        albedo=np.clip(rgb/response,.04,1).astype(np.float32);metal=p.metallic.copy();rough=p.roughness.copy()
        metal[20:29,4:15]=.95;rough[20:29,19:28]=.05
        return replace(p,albedo=albedo,metallic=metal,roughness=rough,confidence=np.ones((h,w),np.float32),
            metadata={**p.metadata,'backend':self.name,'albedo_source':'intrinsic','roughness_source':'synthetic-known',
                      'metallic_source':'synthetic-known','confidence_semantics':'synthetic unit support; not a probability','fallbacks':['normal'],'dummy':True})


def create(output):
    output=Path(output)
    if output.exists():raise ValueError('Use a new output directory')
    output.parent.mkdir(parents=True,exist_ok=True)
    rgb=np.full((99,195,3),145,np.uint8);rgb[6:27,9:45]=1;rgb[6:27,54:84]=255
    rgb[63:87,12:45]=180;rgb[63:87,57:84]=180
    image=output.with_suffix('.png');Image.fromarray(rgb).save(image)
    ReconstructionPipeline(geometry_backend=Geometry(),segmentation_backend=Regions(),material_backend=Materials(),
        lighting_backend=ManualLightingBackend((0,0,1),(1,1,1),(.2,.2,.2)),min_region_pixels=1).run(image,output,max_size=65)
    mask=np.zeros((11,13),np.uint8);mask[7:10,9:12]=255
    Image.fromarray(mask).save(output.with_suffix('.protect.png'))
    Image.fromarray(np.full((3,5),255,np.uint8)).save(output.with_suffix('.all-protect.png'))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output');create(p.parse_args().output)
