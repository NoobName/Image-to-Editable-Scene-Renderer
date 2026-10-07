import json,subprocess,unittest
from pathlib import Path
from dataclasses import replace
from tempfile import TemporaryDirectory
import numpy as np
from shadow_examples import fixture,create
from pipeline.shadow_backend import DepthShellShadowBackend
from shadow_contract import load_shadow
from appearance_contract import load_extension
from scene_package import load_package

class ShadowTests(unittest.TestCase):
    def test_known_occluder_and_dark_texture(self):
        o,truth,dark=fixture();m,_=DepthShellShadowBackend().predict(o);detected=m['candidate']>0
        # Declared before validation: at least 70% of the known projection and no dark-texture false positives.
        self.assertGreater((detected&truth).sum()/truth.sum(),.7)
        self.assertEqual(int(detected[~truth&dark].sum()),0)
        self.assertLess(float(abs(m['visibility'][truth]-.35).max()),1e-6)
        # An asymmetric vertical placement catches a missing camera-Y to image-v sign flip.
        shifted,shift_truth,_=fixture(center_y=.35);s,_=DepthShellShadowBackend().predict(shifted)
        self.assertGreater(((s['candidate']>0)&shift_truth).sum()/shift_truth.sum(),.7)
    def test_missing_offscreen_concavity_sky_emission_are_unknown(self):
        o,truth,_=fixture(True);backend=DepthShellShadowBackend()
        # Missing and offscreen blockers have identical observable geometry: no positive evidence.
        cases=[o,replace(o,labels=np.ones_like(o.labels)),replace(o,excluded=np.ones_like(o.valid)),
               replace(o,roughness=np.zeros_like(o.roughness)),replace(o,metallic=np.ones_like(o.metallic))]
        good,_,_=fixture();cases.append(replace(good,labels=np.ones_like(good.labels))) # same-region AO/concavity
        offscreen,shadow,_=fixture(offscreen=True);self.assertTrue(shadow.any());self.assertTrue((offscreen.labels==1).all());cases.append(offscreen)
        # A source-visible concave depression, with dark AO-like shading, still lacks cross-region evidence.
        z=o.depth+.2*np.exp(-4*o.points[...,0]**2);points=o.points*(z/o.depth)[...,None]
        normal=o.normal.copy();normal[...,0]=-.8*points[...,0]*np.exp(-4*points[...,0]**2);normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
        cases.append(replace(o,depth=z,points=points,normal=normal))
        for case in cases:
            m,_=backend.predict(case);self.assertEqual(m['candidate'].max(),0);self.assertTrue(m['unknown'].all())
    def test_low_quality_and_bad_numbers(self):
        o,_,_=fixture();b=DepthShellShadowBackend()
        for case in (replace(o,uncertainty=np.ones_like(o.depth)),replace(o,error=np.ones_like(o.depth)),replace(o,light_confidence=.01)):
            m,_=b.predict(case);self.assertTrue(m['unknown'].all())
        with self.assertRaises(ValueError):b.predict(replace(o,shading_scale=float('nan')))
        with self.assertRaises(ValueError):b.predict(replace(o,normal=np.full_like(o.normal,float('inf'))))
    def test_versioned_progress(self):
        from pipeline.progress import ProgressReporter,SHADOW_STAGES
        reporter=ProgressReporter(stages=SHADOW_STAGES)
        for stage in SHADOW_STAGES:reporter.stage(stage,'complete')
        reporter.finish();self.assertEqual(reporter.snapshot['stage_order'],['shadow','export'])
    def test_dual_contract_manual_stale_and_invalidation(self):
        from .test_pipeline import VALIDATORS
        from export_analysis import export_saved
        from pipeline.lighting_backend import ManualLightingBackend
        from estimate_intrinsic import export_intrinsic
        from pipeline.intrinsic_backend import SavedIntrinsicBackend
        from estimate_shadows import export_shadows
        with TemporaryDirectory() as td:
            root=Path(td)/'fixtures';create(root);package=root/'manual';path=package/'shadow/shadow.json';good=json.loads(path.read_text(encoding='utf-8'))
            for kind in ('good','version','size','hash','manual','dependency','escape','extra','nan'):
                bad=json.loads(json.dumps(good))
                if kind=='version':bad['version']=2
                if kind=='size':bad['analysisSize'][0]+=1
                if kind=='hash':bad['maps']['candidate']['sha256']='0'*64
                if kind=='manual':bad['manual']['protect']['size'][0]+=1
                if kind=='dependency':bad['inputs'][0]['sha256']='f'*64
                if kind=='escape':bad['maps']['visibility']['path']='shadow/../../outside.dds'
                if kind=='extra':bad['fakeConfidence']=1
                if kind=='nan':bad['parameters']['shadingScale']=float('nan')
                path.write_text(json.dumps(bad),encoding='utf-8')
                if kind=='good':load_package(package)
                else:
                    with self.assertRaises(ValueError,msg=kind):load_package(package)
                for exe in VALIDATORS:
                    r=subprocess.run([str(exe),str(package)],capture_output=True,text=True,encoding='utf-8',errors='replace')
                    self.assertEqual(r.returncode==0,kind=='good',kind+r.stdout+r.stderr)
            path.write_text(json.dumps(good),encoding='utf-8')
            # Geometry can change without changing its min/max or the analysis JSON. Fingerprint
            # the consumed DDS themselves, not just that JSON description.
            from runtime_dds import read_dds,write_dds
            originals={k:(package/f'analysis/{k}.dds').read_bytes() for k in ('depth','position')}
            fmt,z=read_dds(package/'analysis/depth.dds');_,p=read_dds(package/'analysis/position.dds');z[10,10]*=.99;p[10,10,:3]*=.99
            write_dds(package/'analysis/depth.dds',z,fmt);write_dds(package/'analysis/position.dds',p,'RGBA32_FLOAT')
            with self.assertRaises(ValueError):load_package(package)
            for exe in VALIDATORS:
                r=subprocess.run([str(exe),str(package)],capture_output=True);self.assertNotEqual(r.returncode,0)
            for k,bytes_ in originals.items():(package/f'analysis/{k}.dds').write_bytes(bytes_)
            o,_,_=fixture()
            new=export_saved(package,root/'new-light',ManualLightingBackend(o.light['direction'],[.7,.6,.5],[.1,.1,.1]))
            self.assertFalse((new/'shadow').exists());self.assertTrue((package/'shadow').exists())
            # New intrinsic output and source-light output cannot carry old shadow evidence.
            new_iid=export_intrinsic(package,root/'new-iid',SavedIntrinsicBackend(package))
            self.assertFalse((new_iid/'shadow').exists())
            with self.assertRaises(ValueError):export_shadows(package,package/'nested',shading_scale=1)
            for key in ('candidate','visibility','confidence','unknown'):
                self.assertEqual((root/'occluder'/f'shadow/{key}.dds').read_bytes(),(package/f'shadow/{key}.dds').read_bytes())
            rerun=export_shadows(package,root/'rerun',shading_scale=1)
            self.assertEqual((rerun/'shadow/manual_protect.png').read_bytes(),(package/'shadow/manual_protect.png').read_bytes())
            m=load_shadow(package,load_extension(package));self.assertEqual(m['diagnostics']['candidatePixels'],good['diagnostics']['candidatePixels'])
