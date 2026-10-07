"""Known normal-field, multi-material reference fixtures; no model installation/inference."""
import argparse
from dataclasses import replace
from pathlib import Path
import numpy as np
from PIL import Image
from tests.test_lighting import synthetic
from pipeline.geometry_backend import DummyGeometryBackend
from pipeline.material_estimation_backend import NeutralMaterialBackend,quantize,linear_to_srgb
from pipeline.runner import ReconstructionPipeline
from pipeline.reference_backend import rgb_coefficients
from pipeline.lighting_backend import LUMA
from scene_package import write_json_atomic
from match_reference import match


def create(output):
    output=Path(output);output.mkdir(parents=True,exist_ok=False);base,source_direction=synthetic();target_direction=np.array([-.55,.15,.8]);target_direction/=np.linalg.norm(target_direction)
    direct=np.array([.85,.6,.5]);ambient=np.array([.18,.15,.1]);cases={}
    for name in ('source','reference','different','plane'):
        o=base
        if name!='source':
            n=base.normal.copy();a=base.albedo.copy()
            if name=='different':n=n[::-1].copy();a=np.roll(a,11,axis=1)*np.array([.8,1.2,.75],np.float32)
            if name=='plane':n[:]=[0,0,-1]
            rgb=quantize(linear_to_srgb(a*(np.maximum(n@-target_direction,0)[...,None]*direct+ambient)))
            o=replace(base,rgb=rgb,normal=n,albedo=a)
        photo=output/(name+'.png');Image.fromarray(o.rgb).save(photo)
        class Geometry(DummyGeometryBackend):
            def predict(self,image):
                g=super().predict(image);return replace(g,normal=o.normal,confidence=o.valid.astype(np.float32),metadata={**g.metadata,'backend':'synthetic-reference-normal-field'})
        class Material(NeutralMaterialBackend):
            def predict(self,image):
                m=super().predict(image);return replace(m,albedo=o.albedo,confidence=o.material_confidence,metadata={**m.metadata,'albedo_source':'intrinsic','backend':'synthetic-ground-truth'})
        ReconstructionPipeline(geometry_backend=Geometry(),material_backend=Material()).run(photo,output/name,max_size=97,progress=lambda _:None)
    for name,relation in [('reference','same-scene'),('different','different-content'),('plane','different-content')]:
        result=match(output/name,output/'source',output/(name+'-proposal'),relation)
        proposal=result['proposal'];d,b=rgb_coefficients(proposal['target'])
        angle=float(np.degrees(np.arccos(np.clip(np.dot(proposal['target']['direction'],target_direction),-1,1))))
        ratio_error=abs(float(d@LUMA/max(float(b@LUMA),1e-6))/(direct@LUMA/(ambient@LUMA))-1)
        if name!='plane':assert proposal['canApply'] and angle<=3 and ratio_error<=.05,(name,proposal)
        else:assert not proposal['canApply']
        cases[name]={'angleDegrees':angle,'directAmbientRelativeError':ratio_error,'confidence':proposal['confidence'],'canApply':proposal['canApply'],'beforeRMSE':result['fit']['beforeRMSE'],'afterRMSE':result['fit']['afterRMSE']}
    fallback=match(output/'different.png',output/'source',output/'fallback-proposal',geometry_backend='dummy',material_backend='neutral')
    assert not fallback['proposal']['canApply']
    write_json_atomic(output/'truth.json',{'sourceDirection':source_direction.tolist(),'targetDirection':target_direction.tolist(),'direct':direct.tolist(),'ambient':ambient.tolist(),
        'declaredToleranceDegrees':3,'declaredDirectAmbientRelativeTolerance':.05,'cases':cases,'fallbackReasons':fallback['proposal']['reasons']})


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',required=True,type=Path);create(parser.parse_args().output)
