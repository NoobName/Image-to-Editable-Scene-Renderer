import copy
import json
import subprocess
import unittest
from dataclasses import replace
from pathlib import Path
from tempfile import TemporaryDirectory
import numpy as np
from pipeline.types import InputImage
from pipeline.intrinsic_backend import ProxyIntrinsicBackend,IntrinsicEstimate,validate_intrinsic,SavedIntrinsicBackend
from pipeline.adapters.marigold_intrinsic import decode_intrinsic
from pipeline.runner import ReconstructionPipeline
from pipeline.progress import ProgressReporter,INTRINSIC_STAGES
from estimate_intrinsic import export_intrinsic
from scene_package import load_package
from intrinsic_contract import load_intrinsic,read_arrays
from appearance_contract import load_extension
from .support import make_input
from . import test_pipeline

class IntrinsicTests(unittest.TestCase):
    def setUp(self):
        self.image=InputImage(Path('test.png'),np.full((5,7,3),128,np.uint8),(7,5),'0'*64)
        self.props={'target_names':['albedo','shading','residual'],**{k:dict(prediction_space='linear',up_to_scale=k!='albedo') for k in ('albedo','shading','residual')}}
    def test_lighting_channels_stay_linear_and_uncertainty_separate(self):
        a=np.full((3,5,7,3),.5,np.float32);u=np.full_like(a,.1)
        r=decode_intrinsic(self.image,a,u,self.props,{})
        self.assertEqual(r.albedo[0,0,0],.5) # Appearance's sRGB decode here would incorrectly produce .214.
        self.assertAlmostEqual(r.uncertainty[0,0,0],.1)
        self.assertEqual(r.metadata['residual_semantics'],'nonnegative-non-diffuse')
        bad=copy.deepcopy(self.props);bad['albedo']['prediction_space']='srgb'
        with self.assertRaises(ValueError):decode_intrinsic(self.image,a,u,bad,{})
        with self.assertRaises(ValueError):decode_intrinsic(self.image,a[:2],u,self.props,{})
    def test_signed_residual_and_unavailable_are_not_clipped(self):
        r=ProxyIntrinsicBackend().predict(self.image);self.assertIsNone(r.residual)
        signed=replace(r,residual=np.full_like(r.albedo,-.2),metadata={**r.metadata,'residual_semantics':'signed-non-diffuse'})
        validate_intrinsic(self.image,signed);self.assertLess(signed.residual.min(),0)
        with self.assertRaises(ValueError):validate_intrinsic(self.image,replace(signed,metadata={**signed.metadata,'residual_semantics':'nonnegative-non-diffuse'}))
        bad=r.albedo.copy();bad[0,0,0]=np.nan
        with self.assertRaises(ValueError):validate_intrinsic(self.image,replace(r,albedo=bad))
    def test_atomic_saved_reuse_fingerprints_and_both_validators(self):
        with TemporaryDirectory() as td:
            root=Path(td);source=root/'source';target=root/'target'
            ReconstructionPipeline().run(make_input(root/'图.png',(65,33)),source,max_size=65,progress=lambda _:None)
            before={str(p.relative_to(source)):p.read_bytes() for p in source.rglob('*') if p.is_file()}
            export_intrinsic(source,target,ProxyIntrinsicBackend());load_package(target)
            for p,b in before.items():self.assertEqual((target/p).read_bytes(),b,p)
            data=load_intrinsic(target,load_extension(target));self.assertEqual(data['residualSemantics'],'unavailable');self.assertLess(data['metrics']['rmse'],1e-6)
            saved=root/'saved';export_intrinsic(target,saved,SavedIntrinsicBackend(target))
            for k,a in read_arrays(target,data).items():np.testing.assert_array_equal(a,read_arrays(saved,load_intrinsic(saved,load_extension(saved)))[k])
            for executable in test_pipeline.VALIDATORS:self.assertEqual(subprocess.run([str(executable),str(target)],capture_output=True).returncode,0)
            manifest=target/'intrinsic/intrinsic.json';original=manifest.read_text(encoding='utf-8')
            for field,value in (('version',2),('analysisSize',[33,65]),('sourceSha256','0'*64),('colorSpace','srgb')):
                bad=json.loads(original);bad[field]=value;manifest.write_text(json.dumps(bad),encoding='utf-8')
                with self.assertRaises(ValueError):load_package(target)
                for executable in test_pipeline.VALIDATORS:self.assertNotEqual(subprocess.run([str(executable),str(target)],capture_output=True).returncode,0)
            manifest.write_text(original,encoding='utf-8')
            bad=json.loads(original);bad['maps']['shading']['path']='../escape.dds';manifest.write_text(json.dumps(bad),encoding='utf-8')
            with self.assertRaises(ValueError):load_package(target)
            for executable in test_pipeline.VALIDATORS:self.assertNotEqual(subprocess.run([str(executable),str(target)],capture_output=True).returncode,0)
            manifest.write_text(original,encoding='utf-8')
            bad=json.loads(original);bad['metrics']['rmse']=1;manifest.write_text(json.dumps(bad),encoding='utf-8')
            with self.assertRaises(ValueError):load_package(target)
            for executable in test_pipeline.VALIDATORS:self.assertNotEqual(subprocess.run([str(executable),str(target)],capture_output=True).returncode,0)
            with self.assertRaises(ValueError):export_intrinsic(source,source/'nested',ProxyIntrinsicBackend())
            self.assertFalse((source/'nested').exists())
    def test_failure_retains_input_and_versioned_progress(self):
        with TemporaryDirectory() as td:
            root=Path(td);source=root/'source';ReconstructionPipeline().run(make_input(root/'图.png',(33,25)),source,max_size=33,progress=lambda _:None)
            reporter=ProgressReporter(root/'progress.json','intrinsic-test',INTRINSIC_STAGES)
            class Broken(ProxyIntrinsicBackend):
                def predict(self,*args):raise RuntimeError('intentional failure')
            with self.assertRaises(RuntimeError):export_intrinsic(source,root/'failed',Broken(),reporter.stage)
            self.assertFalse((root/'failed').exists());load_package(source)
            reporter.fail('intentional failure');data=json.loads((root/'progress.json').read_text(encoding='utf-8'))
            self.assertEqual(data['stage_order'],['intrinsic','export']);self.assertEqual(data['stages'],{'intrinsic':'error','export':'pending'})
    def test_signed_sidecar_and_missing_uncertainty_roundtrip(self):
        from pipeline.material_estimation_backend import srgb_to_linear
        class Signed(ProxyIntrinsicBackend):
            def predict(self,image,material=None):
                r=super().predict(image,material);linear=srgb_to_linear(image.rgb.astype(np.float32)/255)
                return replace(r,albedo=np.full_like(linear,.5),shading=(2*linear+.4).astype(np.float32),residual=np.full_like(linear,-.2),
                    metadata={**r.metadata,'residual_semantics':'signed-non-diffuse','provenance':'synthetic'})
        with TemporaryDirectory() as td:
            root=Path(td);source=root/'base';ReconstructionPipeline().run(make_input(root/'in.png',(35,27)),source,max_size=35,progress=lambda _:None)
            out=export_intrinsic(source,root/'signed',Signed());d=load_intrinsic(out,load_extension(out));a=read_arrays(out,d)
            self.assertTrue((a['residual'][...,:3]<0).all());self.assertLess(d['metrics']['rmse'],1e-6)
            for exe in test_pipeline.VALIDATORS:self.assertEqual(subprocess.run([str(exe),str(out)],capture_output=True).returncode,0)
        r=decode_intrinsic(self.image,np.zeros((3,5,7,3),np.float32),None,self.props,{})
        np.testing.assert_array_equal(r.uncertainty,np.ones((5,7,3),np.float32))
