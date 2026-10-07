"""Deterministic geometry-only atmosphere fixtures. No trained model or physical haze claim."""
import argparse
from dataclasses import replace
from pathlib import Path
import json
import numpy as np
from PIL import Image
from pipeline.geometry_backend import DummyGeometryBackend, pinhole_points
from pipeline.segmentation_backend import SegmentationBackend
from pipeline.segmentation_types import MaskProposal, SegmentationPrediction
from pipeline.lighting_backend import ManualLightingBackend
from pipeline.runner import ReconstructionPipeline
from pipeline.geometry_builder import GeometryBuilder


class FogGeometry(DummyGeometryBackend):
    name = 'fog-analytic-test'

    def __init__(self, kind):
        super().__init__(); self.kind = kind

    def predict(self, image):
        p = super().predict(image)
        h, w = image.height, image.width
        y, x = np.mgrid[:h, :w]
        rays = pinhole_points(np.ones((h, w), np.float32), p.camera_intrinsics)
        length = np.linalg.norm(rays, axis=-1)
        distance = np.full((h, w), 3, np.float32) if self.kind == 'constant' else (1+5*x/max(w-1, 1)).astype(np.float32)
        z = distance / length
        valid = p.valid_mask.copy()
        if self.kind in ('edge', 'relative', 'zero-confidence'):
            z = np.where(x < w//2, 1., 5.).astype(np.float32)
            valid[h//3:h//2, w//3:w//2] = False
        if self.kind == 'plane':
            z.fill(2)
        z = np.where(valid, z, 0).astype(np.float32)
        conf = valid.astype(np.float32)
        if self.kind == 'zero-confidence':
            conf.fill(0)
        return replace(p, depth=z, point_map=pinhole_points(z, p.camera_intrinsics),
                       normal=np.where(valid[..., None], p.normal, 0).astype(np.float32),
                       valid_mask=valid, confidence=conf,
                       scale_type='metric',
                       metadata={'backend': self.name, 'meaning': 'analytical test distances, not measured geometry'})


class Regions(SegmentationBackend):
    name = 'fog-test-regions'

    def predict(self, image):
        y, x = np.mgrid[:image.height, :image.width]
        sky = y < 10
        return SegmentationPrediction(tuple(MaskProposal(mask, 1, oid, name, category)
            for mask, oid, name, category in (
                (sky, 'sky', 'Sky', 'sky'),
                ((~sky)&(x < image.width//2), 'near', 'Near', 'foreground'),
                ((~sky)&(x >= image.width//2), 'far', 'Far', 'background'))), {'synthetic': True})


def create(root):
    root = Path(root); root.mkdir(parents=True, exist_ok=False)
    y, x = np.mgrid[:195, :259]
    rgb = np.stack((50+70*(x%2), 40+90*(y%2), 55+80*((x+y)%2)), -1).astype(np.uint8)
    image = root/'input.png'; Image.fromarray(rgb).save(image)
    for kind in ('constant', 'gradient', 'plane', 'edge', 'relative', 'zero-confidence'):
        ReconstructionPipeline(geometry_backend=FogGeometry(kind), segmentation_backend=Regions(),geometry_builder=GeometryBuilder(confidence_threshold=0),
            lighting_backend=ManualLightingBackend((0, 0, 1), (.8, .6, .4), (.2, .15, .1))).run(image, root/kind, max_size=129)
        if kind == 'relative':
            # The v1 geometry exporter intentionally refuses unscaled relative meshes. This
            # isolated test changes only the image observation units after exporting its mesh.
            # It is not a production scale adapter or a claim of metric scene reconstruction.
            path = root/kind/'analysis/analysis.json'
            data = json.loads(path.read_text(encoding='utf-8')); data['camera']['scaleType'] = 'relative'
            for key in ('depth', 'position'):
                data['maps'][key]['units'] = 'relative-units'
            path.write_text(json.dumps(data, indent=2), encoding='utf-8')
            path = root/kind/'relighting/relighting.json'
            data = json.loads(path.read_text(encoding='utf-8')); data['sourceCamera']['scaleType'] = 'relative'
            path.write_text(json.dumps(data, indent=2), encoding='utf-8')
    # Missing optional analysis is a legitimate old package, not an invalid scale token.
    import shutil
    shutil.copytree(root/'edge', root/'missing')
    (root/'missing/analysis/analysis.json').rename(root/'missing/analysis/analysis-not-loaded.json')
    (root/'sources.json').write_text(json.dumps({'source': 'Project-authored deterministic pixels and analytical pinhole geometry',
        'sourceSize': [259, 195], 'analysisMaxSize': 129, 'models': 'none', 'kinds': ['constant', 'gradient', 'plane', 'edge', 'relative', 'zero-confidence', 'missing']}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('output'); create(p.parse_args().output)
