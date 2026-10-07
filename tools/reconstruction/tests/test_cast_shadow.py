import unittest
import numpy as np
from cast_shadow_reference import change,decode


class CastShadowTests(unittest.TestCase):
    def setUp(self):
        self.image=np.full((2,3,3),.5);self.n=np.broadcast_to([0,0,-1],self.image.shape)
        self.old=np.ones((2,3,4));self.new=self.old.copy();self.new[...,0]=0
        self.e=np.ones((2,3,4));self.r=np.zeros((2,3,4))
        self.s=dict(direction=[-.6,0,.8],directIntensity=1,directColor=[.7,.6,.5],ambientIntensity=1,ambientColor=[.4]*3)
        self.t={**self.s,'direction':[.6,0,.8]}

    def run_change(self,**kw):return change(self.image,self.n,self.old,self.new,self.e,self.r,self.s,self.t,**kw)[0]

    def test_directional_term_only(self):
        result=decode(self.image)+self.run_change()
        rho=decode(self.image)/(np.array([.4]*3)+.8*np.array([.7,.6,.5]))
        np.testing.assert_allclose(result,rho*.4,rtol=1e-12) # Ambient is preserved, not shadowed to black.

    def test_noops(self):
        for options in ({'strength':0},{'confidence':0},{'enabled':False},{'protection':1}):
            self.assertFalse(self.run_change(**options).any())
        self.t=self.s;self.assertFalse(self.run_change().any())

    def test_emission(self):
        self.r[...,:3]=decode(self.image)
        self.assertFalse(self.run_change().any())

    def test_no_direct(self):
        self.t={**self.t,'directIntensity':0};self.assertFalse(self.run_change().any())

    def test_old_restore_needs_photo_support(self):
        self.old[...,0]=0;self.new[...,0]=1;self.e[...,1]=0
        self.assertFalse(self.run_change().any())
        self.e[...,1]=1;self.e[...,0]=.3
        self.assertTrue((self.run_change()>0).all())

    def test_deep_black_and_invalid(self):
        self.image[:]=0;self.assertFalse(self.run_change().any())
        self.image[:]=.5;self.new[...,1]=0;self.assertFalse(self.run_change().any())

    def test_bounded_and_finite(self):
        self.t={**self.t,'directIntensity':64};delta=self.run_change()
        self.assertTrue(np.isfinite(delta).all());self.assertTrue((abs(delta)<=2*decode(self.image)).all())

    def test_invalid_direction(self):
        self.t={**self.t,'direction':[0,0,0]};self.assertFalse(self.run_change().any())

    def test_unknown_baked_darkness_not_stacked(self):
        self.e[...,0]=.2;self.assertFalse(self.run_change().any())


if __name__=='__main__':unittest.main()
