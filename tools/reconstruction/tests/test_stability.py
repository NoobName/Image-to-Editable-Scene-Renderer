import unittest
import sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from stability_reference import response,sample


class StabilityTests(unittest.TestCase):
    def setUp(self):
        self.p=dict(stability=True,fitCacheValid=True,strength=1,epsilon=.02,ratioClamp=[.25,4],colorMode='bounded-color',chromaLimit=1.5,relativeDepthEdge=.1,normalCosineEdge=.85)
        self.a=np.ones((3,5,4));self.b=self.a.copy();self.b[...,:3]=2;self.q=np.ones_like(self.a);self.src=np.full((3,5,3),145.)
    def test_invariants_and_response(self):
        r,c,*_=response(self.a,self.b,self.q,self.q,self.src,self.p);self.assertGreater(r.mean(),1.9)
        for what in ('identity','strength','confidence','invalid'):
            p=self.p.copy();a=self.a.copy();b=self.b.copy();mask=0
            if what=='identity':b=a.copy()
            if what=='strength':p['strength']=0
            if what=='confidence':mask=1
            if what=='invalid':a[...,3]=0
            r,*_=response(a,b,self.q,self.q,self.src,p,mask);np.testing.assert_array_equal(r,np.ones_like(r))
    def test_black_saturated_reflective(self):
        for value in (0,1,255):
            r,c,*_=response(self.a,self.b,self.q,self.q,np.full_like(self.src,value),self.p);self.assertTrue(np.all(r==1));self.assertTrue(np.all(c==0))
        q=self.q.copy();q[...,2]=0;r,*_=response(self.a,self.b,q,self.q,self.src,self.p);self.assertTrue(np.all(r==1))
    def test_step_ownership_and_rgb_texture_independence(self):
        a=self.a.copy();a[:,3:,:3]=.1;b=a.copy();b[...,:3]*=2
        z=np.full((3,5),3.);z[:,3:]=7;n=np.zeros((3,5,4));n[...,2]=-1;labels=np.ones((3,5),np.uint32);labels[:,3:]=4294967295
        values,good=sample(a,b,self.q,self.q,z,n,labels,9,15,self.p)
        self.assertFalse(good[:,8:10].any());np.testing.assert_allclose(values[0][:,8,:3],1);np.testing.assert_allclose(values[0][:,9,:3],.1)
    def test_near_zero_and_chroma_bounds(self):
        a=self.a.copy();a[...,:3]=1e-12;b=self.b.copy();b[...,:3]=[64,0,.01]
        r,*_=response(a,b,self.q,self.q,self.src,self.p);self.assertTrue(np.isfinite(r).all());self.assertLessEqual(r.max(),1.15*1.5+1e-7);self.assertGreaterEqual(r.min(),.25)
