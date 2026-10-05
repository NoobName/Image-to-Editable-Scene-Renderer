import unittest
import json
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory
from dataclasses import replace
import numpy as np
from pipeline.lighting_backend import LightingInput, ManualLightingBackend, coefficients
from pipeline.lighting_solver import RobustDirectionalAmbientBackend
from pipeline.material_estimation_backend import quantize, linear_to_srgb


def synthetic(direction=(.35, -.3, .88), size=(97, 65)):
    # Known analytic normal field; multicolour material bands are independent of orientation.
    w, h = size; v, u = np.mgrid[:h, :w]
    azimuth = 2*np.pi*(u+.5)/w; z = .05+.9*(v+.5)/h
    n = np.stack((np.sqrt(1-z*z)*np.cos(azimuth), np.sqrt(1-z*z)*np.sin(azimuth), -z), -1).astype(np.float32)
    d = np.array(direction, float); d /= np.linalg.norm(d)
    a = np.array([[.15, .3, .23], [.42, .16, .18], [.22, .23, .45]], np.float32)[(u//7+v//9)%3]
    rgb = quantize(linear_to_srgb(a*(np.maximum(n@-d, 0)[..., None]*[.7, .65, .55]+[.12, .15, .18])))
    scalar = np.ones((h, w), np.float32)
    return LightingInput(rgb, n, a, scalar.astype(bool), scalar*.8, scalar*0, scalar,
        ((u//7+v//9)%3+1).astype(np.uint32), scalar.astype(bool)&False, 'intrinsic',
        {'materialBackend': 'synthetic-ground-truth', 'excludedRegionNames': [], 'normalSpace': 'lh-camera'}), d


class LightingTests(unittest.TestCase):
    def test_multimaterial_known_directions_with_declared_three_degree_tolerance(self):
        for direction in ((.35, -.3, .88), (-.55, .15, .8), (.1, .6, .7)):
            o, d = synthetic(direction)
            result = RobustDirectionalAmbientBackend().predict(o)
            angle = np.degrees(np.arccos(np.clip(np.dot(result.source['direction'], d), -1, 1)))
            self.assertLessEqual(angle, 3.)
            self.assertTrue(result.fit['identifiable'], result.fit)
            self.assertLess(result.fit['afterRMSE'], result.fit['beforeRMSE']*.05)

    def test_degeneracies_are_explicit(self):
        o, _ = synthetic()
        for bad in (replace(o, normal=np.broadcast_to([0., 0., -1.], o.normal.shape)),
                    replace(o, rgb=np.zeros_like(o.rgb)), replace(o, rgb=np.full_like(o.rgb, 255)),
                    replace(o, valid=np.indices(o.valid.shape)[0] == 0)):
            result = RobustDirectionalAmbientBackend().predict(bad)
            self.assertFalse(result.fit['identifiable'])
            self.assertEqual(result.fit['confidence'], 0)
            self.assertTrue(result.fit['reasons'])
            self.assertTrue(np.isfinite(result.old_shading).all())

    def test_neutral_albedo_is_not_claimed_intrinsic(self):
        o, _ = synthetic()
        result = RobustDirectionalAmbientBackend().predict(replace(o, albedo_source='neutral-fallback'))
        self.assertFalse(result.fit['identifiable'])
        self.assertLessEqual(result.fit['confidence'], .1)
        self.assertEqual(result.source['provenance'], 'unreliable-baseline')

    def test_robust_outliers_and_exclusions(self):
        o, d = synthetic(); rgb = o.rgb.copy(); rgb[::9, ::7] = 180
        mask = o.excluded.copy(); mask[:3] = True
        result = RobustDirectionalAmbientBackend().predict(replace(o, rgb=rgb, excluded=mask))
        self.assertLessEqual(np.degrees(np.arccos(np.clip(np.dot(result.source['direction'], d), -1, 1))), 3.)
        self.assertFalse(result.fit_mask[:3].any())
        self.assertGreater(result.fit['excludedCounts']['excluded-region'], 0)

    def test_manual_scale_and_travel_sign(self):
        o, d = synthetic()
        result = ManualLightingBackend(d, [.7, .6, .5], [.1, .2, .3]).predict(o)
        direct, ambient = coefficients(result.source)
        np.testing.assert_allclose(result.old_shading, np.maximum(o.normal@-d, 0)[..., None]*direct+ambient, atol=1e-6)
        self.assertEqual(result.fit['status'], 'manual')
        with self.assertRaises(ValueError):
            ManualLightingBackend([0, 0, 0])

    def test_nonfinite_is_rejected(self):
        o, _ = synthetic(); n = o.normal.copy(); n[0, 0, 0] = np.nan
        with self.assertRaisesRegex(ValueError, 'NaN'):
            RobustDirectionalAmbientBackend().predict(replace(o, normal=n))

    def test_package_contract_python_cpp_and_stale_fingerprint(self):
        from .support import make_input
        from .test_pipeline import VALIDATORS
        from pipeline.runner import ReconstructionPipeline
        from scene_package import load_package
        with TemporaryDirectory() as tmp:
            root = Path(tmp); source = make_input(root/'输入.png', (32, 32)); output = root/'package'
            ReconstructionPipeline().run(source, output, progress=lambda _: None)
            manifest = output/'lighting/lighting.json'; good = json.loads(manifest.read_text(encoding='utf-8'))
            for kind in ('good', 'version', 'direction', 'hash', 'path', 'dimensions', 'support', 'nonfinite', 'identity', 'provenance', 'stale-calibration'):
                value = json.loads(json.dumps(good))
                if kind == 'version': value['version'] = 2
                if kind == 'direction': value['sourceLighting']['direction'] = [0, 0, 0]
                if kind == 'hash': value['inputs'][1]['sha256'] = '0'*64
                if kind == 'path': value['maps']['proxy']['path'] = '../proxy.dds'
                if kind == 'dimensions': value['analysisSize'] = [33, 32]
                if kind == 'support': value['fit']['validPixels'] = 0
                if kind == 'nonfinite': value['fit']['confidence'] = float('nan')
                if kind == 'identity': value['sourceId'] = 'another-input'
                if kind == 'provenance': value['fit']['identifiable'] = True
                if kind == 'stale-calibration': value['sourceLighting']['ambient']['intensity'] += 1
                manifest.write_text(json.dumps(value), encoding='utf-8')
                if kind == 'good': load_package(output)
                else:
                    with self.assertRaises(ValueError, msg=kind): load_package(output)
                for validator in VALIDATORS:
                    run = subprocess.run([str(validator), str(output)], capture_output=True)
                    self.assertEqual(run.returncode == 0, kind == 'good', (kind, run.stderr))
            manifest.write_text(json.dumps(good), encoding='utf-8')
            fingerprinted = output/'analysis/albedo.dds'; original = fingerprinted.read_bytes()
            # Valid DDS, changed reflectance: schema alone cannot detect stale fitted evidence.
            from runtime_dds import read_dds, write_dds
            from lighting_contract import load_lighting
            from appearance_contract import load_extension
            fmt, a = read_dds(fingerprinted); a[..., :3] *= .9; write_dds(fingerprinted, a, fmt)
            with self.assertRaisesRegex(ValueError, 'stale'): load_lighting(output, load_extension(output))
            fingerprinted.write_bytes(original)

    def test_versioned_progress_tables(self):
        from pipeline.progress import ProgressReporter, OFFLINE_STAGES, LEGACY_STAGES
        for version, stages in ((1, LEGACY_STAGES), (2, OFFLINE_STAGES)):
            reporter = ProgressReporter(stages=stages, version=version)
            for stage in stages: reporter.stage(stage, 'complete')
            reporter.finish()
            self.assertEqual(reporter.snapshot['version'], version)
            self.assertEqual('stage_order' in reporter.snapshot, version == 2)
