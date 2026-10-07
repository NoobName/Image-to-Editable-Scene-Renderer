import unittest
from dataclasses import replace
import numpy as np
from pipeline.reference_backend import ReferenceLightingMatcher,parameters,rgb_coefficients
from pipeline.lighting_backend import light_record,LUMA
from pipeline.lighting_optimizer import optimize,evaluate_shading
from .test_lighting import synthetic

class OptimizationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source,cls.initial_direction=synthetic();cls.reference,cls.truth=synthetic((-.55,.15,.8));cls.matcher=ReferenceLightingMatcher()
        cls.baseline=parameters(cls.matcher.analyze(cls.source).estimate.source)
        cls.analysis={'proposal':cls.matcher.propose(cls.matcher.analyze(cls.reference),cls.baseline,'same-scene')}

    def test_known_light_reduces_fixed_loss_and_recovers_parameters(self):
        before=repr(self.source);r=optimize(self.source,self.reference,self.analysis,self.baseline,self.baseline,'same-scene',registered=True)
        self.assertEqual(r.report['status'],'improved',r.report);self.assertLess(r.report['bestLoss'],r.report['initialLoss']*.01)
        self.assertLess(np.degrees(np.arccos(np.clip(np.dot(r.target['direction'],self.truth),-1,1))),3)
        d,b=rgb_coefficients(r.target);td,tb=rgb_coefficients(self.analysis['proposal']['target'])
        self.assertLess(abs(float(d@LUMA/(b@LUMA))/float(td@LUMA/(tb@LUMA))-1),.05)
        self.assertEqual(repr(self.source),before);self.assertEqual(r.report['exposureDeltaBounds'],[0,0])
        self.assertTrue(all(h['bestLoss']>=n['bestLoss'] for h,n in zip(r.report['history'],r.report['history'][1:])))

    def test_different_content_ignores_reference_pixel_rgb(self):
        a=optimize(self.source,self.reference,self.analysis,self.baseline,self.baseline,'different-content')
        b=optimize(self.source,replace(self.reference,rgb=np.zeros_like(self.reference.rgb)),self.analysis,self.baseline,self.baseline,'different-content')
        self.assertEqual(a.target,b.target);self.assertEqual(a.report['history'],b.report['history']);self.assertEqual(a.report['status'],'improved')

    def test_masks_wrong_reference_plane_cancel_and_limits(self):
        for kwargs in ({'registered':False},{'registered':True,'protection':np.ones_like(self.source.valid,float)},{'registered':True,'cancel':lambda:True}):
            r=optimize(self.source,self.reference,self.analysis,self.baseline,self.baseline,'same-scene',**kwargs)
            self.assertNotEqual(r.report['status'],'improved');self.assertEqual(r.target,self.baseline)
        wrong=replace(self.reference,albedo=1-self.reference.albedo)
        self.assertEqual(optimize(self.source,wrong,self.analysis,self.baseline,self.baseline,'same-scene',registered=True).report['status'],'rejected')
        plane=replace(self.source,normal=np.broadcast_to([0,0,-1],self.source.normal.shape).astype(np.float32))
        self.assertEqual(optimize(plane,self.reference,self.analysis,self.baseline,self.baseline,'different-content').report['status'],'rejected')
        opposite={**self.baseline,'direction':(-np.array(self.analysis['proposal']['target']['direction'])).tolist()}
        self.assertEqual(optimize(self.source,self.reference,self.analysis,opposite,self.baseline,'different-content').report['status'],'rejected')
        with self.assertRaises(ValueError):optimize(self.source,self.reference,self.analysis,self.baseline,self.baseline,'different-content',iterations=201)

    def test_cancel_during_search_and_no_improvement_preserve_initial(self):
        calls=[0]
        def cancel():calls[0]+=1;return calls[0]>3
        r=optimize(self.source,self.reference,self.analysis,self.baseline,self.baseline,'different-content',cancel=cancel)
        self.assertEqual(r.report['status'],'cancelled');self.assertEqual(r.target,self.baseline)
        target=self.analysis['proposal']['target'];r=optimize(self.source,self.reference,self.analysis,target,self.baseline,'different-content')
        self.assertEqual(r.report['status'],'no-improvement');self.assertEqual(r.target,target)
        values=evaluate_shading(self.source.normal,np.zeros_like(self.source.valid),target);self.assertTrue(np.all(values==0))

if __name__=='__main__':unittest.main()
