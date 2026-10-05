"""Reproducible Lambert fitting evidence. Normal-field tests, not claims of full inverse rendering."""
import argparse
from pathlib import Path
import time
import numpy as np
from PIL import Image
from dataclasses import replace
from tests.test_lighting import synthetic
from pipeline.geometry_backend import DummyGeometryBackend
from pipeline.material_estimation_backend import NeutralMaterialBackend
from pipeline.runner import ReconstructionPipeline
from pipeline.lighting_solver import RobustDirectionalAmbientBackend
from scene_package import write_json_atomic


def make_examples(root):
    root = Path(root); root.mkdir(parents=True, exist_ok=False)
    cases = {}; o, truth = synthetic()
    for name, obs in [('lambert', o), ('plane', replace(o, normal=np.broadcast_to([0., 0., -1.], o.normal.shape).copy().astype(np.float32))),
                      ('black', replace(o, rgb=np.zeros_like(o.rgb))), ('overexposed', replace(o, rgb=np.full_like(o.rgb, 255))),
                      ('low-validity', replace(o, valid=np.indices(o.valid.shape)[0] < 3)),
                      ('neutral', replace(o, albedo=np.full_like(o.albedo, .5), albedo_source='neutral-fallback'))]:
        source = root / (name+'.png'); Image.fromarray(obs.rgb).save(source)
        class Geometry(DummyGeometryBackend):
            def predict(self, image):
                g = super().predict(image); n = obs.normal.copy(); n[~obs.valid] = 0
                p = g.point_map.copy(); p[~obs.valid] = 0; depth = g.depth.copy(); depth[~obs.valid] = 0
                return replace(g, normal=n, point_map=p, depth=depth, valid_mask=obs.valid, confidence=obs.valid.astype(np.float32),
                    metadata={**g.metadata, 'backend': 'synthetic-normal-field', 'fixture': name})
        class Material(NeutralMaterialBackend):
            def predict(self, image):
                m = super().predict(image)
                return replace(m, albedo=obs.albedo, confidence=obs.material_confidence,
                    metadata={**m.metadata, 'albedo_source': obs.albedo_source, 'backend': 'synthetic-ground-truth'})
        started = time.perf_counter()
        ReconstructionPipeline(geometry_backend=Geometry(), material_backend=Material()).run(source, root/name, max_size=97)
        result = RobustDirectionalAmbientBackend().predict(obs)
        angle = float(np.degrees(np.arccos(np.clip(np.dot(result.source['direction'], truth), -1, 1))))
        cases[name] = {'directionErrorDegrees': angle, 'declaredToleranceDegrees': 3, 'fit': result.fit, 'seconds': time.perf_counter()-started}
    write_json_atomic(root/'synthetic-results.json', cases)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('output', type=Path)
    make_examples(parser.parse_args().output)
