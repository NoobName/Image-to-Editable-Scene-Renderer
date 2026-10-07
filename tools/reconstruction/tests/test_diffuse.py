import unittest
from dataclasses import replace
import numpy as np
from .test_lighting import synthetic
from pipeline.diffuse_backend import IntrinsicAssistedBackend
from pipeline.lighting_solver import RobustDirectionalAmbientBackend
from pipeline.lighting_backend import coefficients
from pipeline.intrinsic_backend import IntrinsicEstimate, IntrinsicBackend

def fixture():
    o,d=synthetic(); shading=(np.maximum(o.normal@-d,0)[...,None]*[.7,.65,.55]+[.12,.15,.18]).astype(np.float32)
    maps=dict(albedo=o.albedo,shading=shading,residual=np.zeros_like(shading),uncertainty=np.zeros_like(shading),validity=o.valid.astype(np.uint32))
    metadata={'provenance':{'provenance':'synthetic'}}
    biased=replace(o,albedo=o.albedo*(.65+.3*o.normal[...,0:1]))
    return biased,d,maps,metadata

class KnownIntrinsic(IntrinsicBackend):
    name='synthetic-diffuse'
    def predict(self,image,material=None):
        _,_,m,_=fixture()
        return IntrinsicEstimate(**m,metadata=dict(backend=self.name,residual_semantics='nonnegative-non-diffuse',uncertainty_semantics='synthetic known zero',gauge='known-lambert',scale_ambiguous=False,provenance='synthetic'))

def metrics(o,d,m):
    base=RobustDirectionalAmbientBackend().predict(o); assist=IntrinsicAssistedBackend(m,{'provenance':{'provenance':'synthetic'}}).predict(o)
    results={}
    target=(np.maximum(o.normal@-np.array([-.5,.2,.84])/np.linalg.norm([-.5,.2,.84]),0)[...,None]*[.65,.4,.8]+[.18,.2,.12])
    original=m['albedo']*m['shading']; expected=m['albedo']*target
    for key,fit in [('baseline',base),('assisted',assist)]:
        direct,ambient=coefficients(fit.source); scale=fit.fit['normalization']
        old=fit.old_shading*scale
        image=original*(target+.02)/(old+.02)
        results[key]={'angle':float(np.degrees(np.arccos(np.clip(np.dot(fit.source['direction'],d),-1,1)))),
            'coefficientError':float(np.max(np.abs(np.r_[direct*scale,ambient*scale]-[.7,.65,.55,.12,.15,.18]))),
            'targetRMSE':float(np.sqrt(np.mean((image-expected)**2)))}
    return results

class DiffuseTests(unittest.TestCase):
    def test_synthetic_improves_predeclared_tolerances(self):
        o,d,m,_=fixture(); r=metrics(o,d,m)
        self.assertLess(r['assisted']['angle'],3)
        self.assertLess(r['assisted']['coefficientError'],.02)
        self.assertLess(r['assisted']['targetRMSE'],.015)
        for k in r['assisted']:self.assertLess(r['assisted'][k],r['baseline'][k])
    def test_missing_proxy_and_black_fallback(self):
        o,_,m,meta=fixture()
        cases=[IntrinsicAssistedBackend(),IntrinsicAssistedBackend(m,{'provenance':{'provenance':'derived-not-estimated'}}),
               IntrinsicAssistedBackend({**m,'albedo':np.zeros_like(m['albedo'])},meta)]
        for b in cases:
            r=b.predict(o);self.assertFalse(r.assistance['selected']);self.assertTrue(np.isfinite(r.old_shading).all())
    def test_flipped_normals_have_documented_sign_ambiguity(self):
        o,d,m,meta=fixture();r=IntrinsicAssistedBackend(m,meta).predict(replace(o,normal=-o.normal))
        # A simultaneous normal/light sign reversal is unidentifiable from Lambert shading alone.
        self.assertLess(np.degrees(np.arccos(np.clip(np.dot(r.source['direction'],-d),-1,1))),3)
    def test_high_residual_and_uncertainty_protect(self):
        from pipeline.diffuse_backend import diffuse_support
        o,_,m,_=fixture(); clean,_,_=diffuse_support(o,m)
        dirty,_,_=diffuse_support(o,{**m,'residual':np.ones_like(m['residual'])})
        uncertain,_,_=diffuse_support(o,{**m,'uncertainty':np.ones_like(m['uncertainty'])})
        self.assertGreater(clean.mean(),.95);self.assertEqual(dirty.max(),0);self.assertLess(uncertain.max(),.002)

    def test_dual_contract_stale_intrinsic_and_invalidation(self):
        import json, subprocess
        from pathlib import Path
        from tempfile import TemporaryDirectory
        from .support import make_input
        from .test_pipeline import VALIDATORS
        from pipeline.runner import ReconstructionPipeline
        from pipeline.intrinsic_backend import ProxyIntrinsicBackend
        from estimate_intrinsic import export_intrinsic
        from export_analysis import export_saved
        from scene_package import load_package
        with TemporaryDirectory() as td:
            root=Path(td);base=root/'base'
            ReconstructionPipeline().run(make_input(root/'input.png',(33,25)),base,max_size=33,progress=lambda _:None)
            intrinsic=export_intrinsic(base,root/'intrinsic',ProxyIntrinsicBackend())
            out=export_saved(intrinsic,root/'fit',IntrinsicAssistedBackend())
            with self.assertRaises(ValueError):export_saved(out,out/'nested',IntrinsicAssistedBackend())
            path=out/'lighting/lighting.json';good=json.loads(path.read_text(encoding='utf-8'))
            for kind in ('good','hash','selected','scale','source'):
                bad=json.loads(json.dumps(good))
                if kind=='hash':bad['assistance']['intrinsicSha256']='0'*64
                if kind=='selected':bad['assistance']['selected']=True
                if kind=='scale':bad['assistance']['colorGain']=[1,2,1]
                if kind=='source':bad['assistance']['baselineSource']['direction']=[0,0,0]
                path.write_text(json.dumps(bad),encoding='utf-8')
                if kind=='good':load_package(out)
                else:
                    with self.assertRaises(ValueError):load_package(out)
                for exe in VALIDATORS:self.assertEqual(subprocess.run([str(exe),str(out)],capture_output=True).returncode==0,kind=='good')
            path.write_text(json.dumps(good),encoding='utf-8')
            updated=export_intrinsic(out,root/'updated',ProxyIntrinsicBackend())
            self.assertFalse((updated/'lighting/lighting.json').exists())
            self.assertTrue(path.exists());load_package(out);load_package(updated)
