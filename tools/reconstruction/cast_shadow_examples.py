"""Prompt27 analytic fixtures. No models, no overwrite, no new dependencies."""
from pathlib import Path
from dataclasses import replace
import argparse
import numpy as np
from PIL import Image
from shadow_examples import fixture
from pipeline.geometry_backend import DummyGeometryBackend
from pipeline.material_estimation_backend import NeutralMaterialBackend,linear_to_srgb,quantize
from pipeline.segmentation_backend import SegmentationBackend
from pipeline.segmentation_types import SegmentationPrediction,MaskProposal
from pipeline.lighting_backend import ManualLightingBackend,coefficients,light_record
from pipeline.intrinsic_backend import IntrinsicBackend,IntrinsicEstimate
from pipeline.runner import ReconstructionPipeline
from pipeline.geometry_builder import GeometryBuilder
from estimate_intrinsic import export_intrinsic
from estimate_shadows import export_shadows


def create(root):
    root=Path(root);root.mkdir(exist_ok=False)
    for kind in ('plane','missing','emission','black','no-confidence','asymmetric'):
        o,truth,_=fixture(missing=kind=='missing',center_y=.22 if kind=='asymmetric' else 0.)
        h,w=o.depth.shape;y,x=np.mgrid[:h,:w]
        albedo=np.broadcast_to([.4,.3,.2],o.points.shape).copy().astype(np.float32)
        albedo[(x//9+y//7)%2==0]*=.8
        direct=np.array([.7,.6,.5]);ambient=np.array([.4,.4,.4]);travel=np.array([-.6,0,.8])
        visibility=np.where(truth,0,1).astype(np.float32)
        shading=(ambient+.8*direct*visibility[...,None]).astype(np.float32)
        residual=np.zeros_like(albedo)
        if kind=='emission':residual[:]=[.25,.2,.15];albedo[:]=0
        if kind=='black':albedo[:]=0
        o=replace(o,albedo=albedo,shading=shading,light=light_record(-travel,direct,ambient,'manual'))
        Image.fromarray(quantize(linear_to_srgb(albedo*shading+residual))).save(root/(kind+'.png'))
        class Geometry(DummyGeometryBackend):
            def predict(self,image):
                g=super().predict(image)
                return replace(g,normal=o.normal,point_map=o.points,depth=o.depth,valid_mask=o.valid,
                    confidence=np.full_like(o.depth,0 if kind=='no-confidence' else 1),camera_intrinsics=o.intrinsics,
                    metadata={**g.metadata,'backend':'synthetic-cast-shadow-'+kind})
        class Material(NeutralMaterialBackend):
            def predict(self,image):
                m=super().predict(image)
                return replace(m,albedo=albedo,roughness=o.roughness,metallic=o.metallic,confidence=np.ones_like(o.depth),
                    metadata={**m.metadata,'backend':'synthetic-known-material','albedo_source':'intrinsic'})
        class Segmentation(SegmentationBackend):
            name='synthetic-cast-shadow-regions'
            def predict(self,image):
                return SegmentationPrediction(tuple(MaskProposal(o.labels==i,object_id=f'object-{i}',name='Receiver' if i==1 else 'Blocker',category='ground' if i==1 else 'major') for i in np.unique(o.labels)))
        class Intrinsic(IntrinsicBackend):
            name='synthetic-cast-shadow'
            def predict(self,image,material=None):
                return IntrinsicEstimate(albedo,shading,residual,np.zeros_like(albedo),o.valid.astype(np.uint32),
                    dict(backend=self.name,residual_semantics='nonnegative-non-diffuse',uncertainty_semantics='synthetic-known',gauge='known-Lambert',scale_ambiguous=False,provenance='synthetic'))
        ReconstructionPipeline(geometry_backend=Geometry(),segmentation_backend=Segmentation(),material_backend=Material(),geometry_builder=GeometryBuilder(confidence_threshold=0),
            lighting_backend=ManualLightingBackend(travel,direct,ambient)).run(root/(kind+'.png'),root/(kind+'-base'),max_size=w)
        export_intrinsic(root/(kind+'-base'),root/(kind+'-intrinsic'),Intrinsic())
        export_shadows(root/(kind+'-intrinsic'),root/kind,shading_scale=1)
        target_truth=(abs(o.points[...,0]-.9)<.25)&(abs(o.points[...,1]-(.22 if kind=='asymmetric' else 0))<.35)&(o.depth>2)
        np.savez_compressed(root/(kind+'-truth.npz'),old=truth,new=target_truth,albedo=albedo,residual=residual,
            source=albedo*shading+residual,target=albedo*(ambient+.8*direct*(~target_truth)[...,None])+residual,points=o.points)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',default='generated/prompt27-fixtures')
    create(parser.parse_args().output)
