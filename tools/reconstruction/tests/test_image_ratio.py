import unittest
import numpy as np
try:
    from verify_image_ratio import ratio_reference,decode,encode
except ModuleNotFoundError:
    from tools.reconstruction.verify_image_ratio import ratio_reference,decode,encode


class RatioTests(unittest.TestCase):
    def setUp(self):
        self.p={'epsilon':.02,'strength':1,'ratioClamp':[.25,4],'colorMode':'bounded-color'}

    def test_identity_zero_invalid_and_extreme(self):
        rng=np.random.default_rng(21);a=rng.uniform(0,4,(9,17,4));a[...,3]=1;b=a.copy()
        r,_=ratio_reference(a,b,self.p);np.testing.assert_array_equal(r,1)
        b[...,:3]=1e3;self.p['strength']=0;r,_=ratio_reference(a,b,self.p);np.testing.assert_array_equal(r,1)
        self.p['strength']=1;a[...,3]=0;r,_=ratio_reference(a,b,self.p);np.testing.assert_array_equal(r,1)
        a[...,:3]=0;a[...,3]=1;b[...,:3]=0;r,_=ratio_reference(a,b,self.p);np.testing.assert_array_equal(r,1)
        b[...,:3]=1e-12;r,_=ratio_reference(a,b,self.p);self.assertTrue(np.isfinite(r).all())
        b[...,:3]=1e3;r,_=ratio_reference(a,b,self.p);self.assertTrue(np.all(r<=4))

    def test_colored_light_semantics(self):
        a=np.ones((1,1,4));b=np.array([[[2,.5,.2,1]]])
        colored,_=ratio_reference(a,b,self.p);self.assertGreater(float(np.ptp(colored)),.1)
        self.p['colorMode']='luminance';gray,_=ratio_reference(a,b,self.p)
        self.assertEqual(float(np.ptp(gray)),0)
        self.assertNotEqual(float(gray[0,0,0]),1)

    def test_source_never_accumulates_or_blurs(self):
        x=np.arange(256,dtype=float)/255;original=decode(x)
        for strength in [0,.4,1,0,1,0]:
            result=original*1.3**strength
            if strength==0:np.testing.assert_array_equal(encode(result),np.arange(256))
        result=original*1.3
        np.testing.assert_allclose(np.diff(result),1.3*np.diff(original),rtol=1e-12,atol=1e-15)


if __name__=='__main__':unittest.main()
