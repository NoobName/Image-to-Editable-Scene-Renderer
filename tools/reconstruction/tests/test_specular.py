import unittest
import numpy as np
from specular_reference import surface,ggx
class SpecularTests(unittest.TestCase):
    def test_fixed_view_peak_movement_and_roughness(self):
        for kind in ('plane','sphere'):
            p,n,_,valid,_=surface(kind);a=np.full_like(p,.25);r=np.full(valid.shape,.32);m=np.zeros_like(r)
            old=ggx(n,p,a,r,m,[.25,-.15,1],[.65,.6,.5]);new=ggx(n,p,a,r,m,[-.35,.1,1],[.65,.6,.5])
            wide=ggx(n,p,a,r*1.7,m,[-.35,.1,1],[.65,.6,.5])
            self.assertNotEqual(np.argmax(old[...,0]),np.argmax(new[...,0]))
            self.assertLess(wide[...,0].max(),new[...,0].max())
            self.assertTrue(np.isfinite(old).all());self.assertTrue((old[~valid]==0).all())
    def test_original_separation_recomposes_and_delta_is_bounded(self):
        rng=np.random.default_rng(25);original=rng.random((19,31,3));candidate=np.minimum(rng.random(original.shape),.6*original)
        anchor=original-candidate;np.testing.assert_allclose(anchor+candidate,original,atol=1e-15)
        delta=np.clip(rng.normal(size=original.shape),-candidate,.15*original+.02)
        result=original+candidate*(1-1)+delta
        self.assertTrue((result>=0).all());self.assertTrue((delta<=.15*original+.02).all())
        np.testing.assert_array_equal(original+candidate*(1-1),original)
