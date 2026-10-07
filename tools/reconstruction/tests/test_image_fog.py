import unittest
import numpy as np
import json
from pathlib import Path
from package_schema import validate, _check_profile
from fog_reference import evaluate, sample


class ImageFogTests(unittest.TestCase):
    def test_shared_recipe_fog_schema(self):
        schema = json.loads((Path(__file__).resolve().parents[3]/'schemas/relighting-recipe.schema.json').read_text(encoding='utf-8'))
        _check_profile(schema)
        fog = dict(version=1, sourceAdditionalDensity=0, enabled=True, density=.3, airlightLinear=[.7, .8, 1], allowRelativeScale=False)
        validate(fog, schema['$defs']['imageFog'], schema=schema)
        for key, value in [('density', -1), ('density', 1001), ('density', float('nan')), ('sourceAdditionalDensity', 1), ('version', 2), ('airlightLinear', [1, 1, 2])]:
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                validate(dict(fog, **{key: value}), schema['$defs']['imageFog'], schema=schema)

    def test_known_distance(self):
        samples = np.zeros((3, 5, 4)); samples[..., 0] = 2; samples[..., 1:] = 1
        color = np.full((3, 5, 3), .2)
        result, t, _ = evaluate(color, samples, .5, [.8, .8, .8])
        np.testing.assert_allclose(t, np.exp(-1)); np.testing.assert_allclose(result, .2*np.exp(-1)+.8*(1-np.exp(-1)))

    def test_zero_and_extreme(self):
        s = np.array([[[1e6, 1, 1, 1]]]); c = np.full((1, 1, 3), .3)
        np.testing.assert_array_equal(evaluate(c, s, 0, [1, 1, 1])[0], c)
        self.assertTrue(np.isfinite(evaluate(c, s, 1000, [1, 1, 1])[0]).all())
        np.testing.assert_array_equal(evaluate(c, s, 1000, [1, 1, 1], protection=1)[0], c)

    def test_invalid_boundary(self):
        data = np.zeros((3, 5, 4)); data[:, :2] = [2, 1, 2, 1]; labels = np.ones((3, 5), int)
        s = sample(data, labels, 259, 195)
        self.assertTrue(np.all(np.isclose(s[..., 0], 0, atol=1e-12)|np.isclose(s[..., 0], 2, atol=1e-12)))

    def test_cross_label_and_depth_step(self):
        data = np.zeros((3, 5, 4)); data[:, :2] = [1, 1, 1, 1]; data[:, 2:] = [5, 1, 5, 1]
        s = sample(data, np.ones((3, 5), int), 259, 195)
        self.assertTrue(np.all(np.isclose(s[..., 0], 1, atol=1e-12)|np.isclose(s[..., 0], 5, atol=1e-12)))
        data[:, 2:, 2] = 1
        labels = np.ones((3, 5), int); labels[:, 2:] = 2
        s = sample(data, labels, 259, 195)
        self.assertTrue(np.all(np.isclose(s[..., 0], 1, atol=1e-12)|np.isclose(s[..., 0], 5, atol=1e-12)))
