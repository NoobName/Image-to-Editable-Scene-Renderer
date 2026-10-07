"""Contract tests only. No fake backend is counted as a pretrained-model acceptance."""
import copy
import unittest
import numpy as np
from refinement_io import accept_candidate
from pipeline.adapters.pixl_refinement import decode
from pipeline.neural_refinement_backend import RefinementInput, RefinementParameters, run_guarded, validate_input


def fixture():
    light={'direction':[0.,0.,1.], 'directColor':[1.,1.,1.], 'ambientColor':[1.,1.,1.],
           'directIntensity':1., 'ambientIntensity':.2}
    target=copy.deepcopy(light); target['direction']=[1.,0.,0.]
    scalar=np.ones((3,5),np.float32)
    vector=np.zeros((3,5,3),np.float32); vector[:,:,2]=-1
    return RefinementInput(np.zeros((6,10,3),np.uint8),np.full((6,10,3),.125,np.float32),light,target,
        {'depth':scalar.copy(),'normal':vector,'oldShading':np.ones_like(vector),'newShading':np.ones_like(vector)*2,
         'validity':np.ones((3,5),bool),'confidence':scalar.copy(),'relightingConfidence':np.ones((6,10),np.float32)},np.ones((6,10),np.float32),'a'*64,'b'*64,'relative')


class RefinementContractTests(unittest.TestCase):
    def test_identity_pixels_bypass_changed_light_parameters(self):
        f=fixture(); f.physics_rgb[:]=decode(f.original_rgb)
        self.assertEqual(run_guarded(f,RefinementParameters(.5)).provenance['reason'],'physics-identity')

    def test_gate_protects_explicit_pixels_and_never_modifies_inputs(self):
        f=fixture(); f.original_rgb[:]=128; f.protection[:]=0; f.protection[:,0]=1
        candidate=f.physics_rgb+.02; before=f.physics_rgb.copy()
        result,weight,_,_,metrics=accept_candidate(f,candidate,.5)
        self.assertTrue(np.array_equal(result[:,0],f.physics_rgb[:,0]))
        self.assertGreater(weight[:,1:].min(),0)
        self.assertTrue(np.allclose(result[:,1:],before[:,1:]+.01))
        self.assertTrue(np.array_equal(before,f.physics_rgb)); self.assertEqual(metrics['protectedMaximumError'],0)

    def test_lost_signal_invalid_low_confidence_reject(self):
        for which in ('black','white','invalid','low-confidence','large-change','physics-protected'):
            f=fixture();f.original_rgb[:]=128;f.protection[:]=0
            if which=='black':f.original_rgb[:]=0
            if which=='white':f.original_rgb[:]=255
            if which=='invalid':f.guidance['validity'][:]=False
            if which=='low-confidence':f.guidance['confidence'][:]=.1
            if which=='physics-protected':f.guidance['relightingConfidence'][:]=0
            candidate=f.physics_rgb+(.3 if which=='large-change' else .01)
            result,weight,_,_,_=accept_candidate(f,candidate,.5)
            self.assertEqual(weight.max(),0,which)
            self.assertTrue(np.array_equal(result,f.physics_rgb),which)

    def test_strict_optional_recipe_schema(self):
        from refine_image import SCHEMA
        from package_schema import validate
        with self.assertRaises(ValueError):validate({'version':2},SCHEMA,schema=SCHEMA)

    def test_zero_bypasses_without_model_import(self):
        f=fixture(); result=run_guarded(f,RefinementParameters())
        self.assertTrue(np.array_equal(result.rgb,f.physics_rgb)); self.assertEqual(result.provenance['reason'],'strength-zero')

    def test_unchanged_lighting_bypasses(self):
        f=fixture(); f.target_lighting.update(f.source_lighting)
        self.assertEqual(run_guarded(f,RefinementParameters(.5)).provenance['reason'],'lighting-unchanged')

    def test_missing_model_explicit_not_success(self):
        f=fixture(); result=run_guarded(f,RefinementParameters(.5))
        self.assertEqual(result.provenance['status'],'unavailable'); self.assertFalse(result.provenance['neuralInference'])
        self.assertTrue(np.array_equal(result.rgb,f.physics_rgb))

    def test_cancel_retains_physics(self):
        f=fixture(); result=run_guarded(f,RefinementParameters(.5),cancelled=lambda:True)
        self.assertEqual(result.provenance['reason'],'cancelled'); self.assertTrue(np.array_equal(result.rgb,f.physics_rgb))

    def test_rejects_nonfinite_and_nonunit_geometry(self):
        f=fixture(); f.guidance['normal'][0,0]=0
        with self.assertRaises(ValueError):validate_input(f)
        f=fixture(); f.physics_rgb[0,0,0]=np.nan
        with self.assertRaises(ValueError):validate_input(f)

    def test_budgets_and_parameters(self):
        for p in (RefinementParameters(float('inf')),RefinementParameters(-1),RefinementParameters(.5,-1),RefinementParameters(.5,34,1024)):
            with self.assertRaises(ValueError):p.validate()

    def test_backend_failure_retains_inputs(self):
        class DeliberateFailure:
            def predict(self,*args):raise RuntimeError('test-only failure; no neural model')
        f=fixture(); src=f.original_rgb.copy(); physics=f.physics_rgb.copy()
        result=run_guarded(f,RefinementParameters(.5),DeliberateFailure())
        self.assertEqual(result.provenance['status'],'fallback')
        self.assertTrue(np.array_equal(result.rgb,physics)); self.assertTrue(np.array_equal(f.original_rgb,src))


if __name__=='__main__':unittest.main()
