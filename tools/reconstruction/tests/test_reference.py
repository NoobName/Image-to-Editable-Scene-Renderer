import unittest
from dataclasses import replace
import numpy as np
from pipeline.reference_backend import ReferenceLightingMatcher,parameters,rgb_coefficients
from pipeline.lighting_backend import light_record,LUMA
from .test_lighting import synthetic


class ReferenceTests(unittest.TestCase):
    def test_registered_and_different_content_recover_illumination_not_rgb(self):
        source=parameters(light_record(np.array([-.35,.3,-.88])/np.linalg.norm([.35,-.3,.88]),[.7,.65,.55],[.12,.15,.18],'manual-test'))
        matcher=ReferenceLightingMatcher();reference,truth=synthetic((-.55,.15,.8));before=repr(source)
        for relation in ('same-scene','different-content'):
            analysis=matcher.analyze(reference);p=matcher.propose(analysis,source,relation)
            self.assertTrue(p['canApply']);self.assertLessEqual(np.degrees(np.arccos(np.clip(np.dot(p['target']['direction'],truth),-1,1))),3)
            d,b=rgb_coefficients(p['target']);expected=np.array([.7,.65,.55])@LUMA/(np.array([.12,.15,.18])@LUMA)
            self.assertLess(abs(float(d@LUMA/(b@LUMA))/expected-1),.05)
            sd,sb=rgb_coefficients(source);self.assertAlmostEqual(float((d+b)@LUMA),float((sd+sb)@LUMA),places=6)
            self.assertEqual(p['exposureEV'],0);self.assertEqual(repr(source),before)

    def test_neutral_colors_and_rank_deficiency_are_not_light_estimates(self):
        obs,_=synthetic();source=parameters(light_record([0,0,-1],[.8,.7,.6],[.1,.2,.3],'manual-test'));matcher=ReferenceLightingMatcher()
        for bad in (replace(obs,albedo_source='neutral-fallback'),replace(obs,normal=np.broadcast_to([0,0,-1],obs.normal.shape).astype(np.float32)),replace(obs,rgb=np.zeros_like(obs.rgb)),replace(obs,valid=np.zeros_like(obs.valid))):
            p=matcher.propose(matcher.analyze(bad),source)
            self.assertFalse(p['canApply']);self.assertEqual(p['confidence'],0);self.assertEqual(p['target'],source);self.assertTrue(p['unavailable'])

    def test_zero_source_energy_and_adapter_fallback_are_explicit(self):
        obs,_=synthetic();matcher=ReferenceLightingMatcher();source=parameters(light_record([0,0,-1],[0,0,0],[0,0,0],'manual-test'))
        p=matcher.propose(matcher.analyze(obs),source);self.assertFalse(p['canApply'])
        source['directIntensity']=1;p=matcher.propose(matcher.analyze(obs,{'fallbackReasons':['missing-cached-model']}),source)
        self.assertFalse(p['canApply']);self.assertIn('missing-cached-model',p['reasons'])


if __name__=='__main__':unittest.main()
