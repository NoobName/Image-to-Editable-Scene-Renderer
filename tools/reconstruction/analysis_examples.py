"""Deterministic numeric fixtures: plane, tilted plane, discontinuity, invalid pixels and odd sizes."""
import argparse
from pathlib import Path
import numpy as np
from PIL import Image
from pipeline.geometry_backend import GeometryEstimationBackend, GeometryPrediction, pinhole_points, validate_prediction
from pipeline.runner import ReconstructionPipeline


class SyntheticGeometry(GeometryEstimationBackend):
    name = "analysis-fixture"

    def __init__(self, kind):
        self.kind = kind

    def predict(self, image):
        h, w = image.height, image.width
        k = np.array([[1., 0, .5], [0, w/h, .5], [0, 0, 1]], np.float32)
        rays = pinhole_points(np.ones((h, w), np.float32), k)
        depth = np.full((h, w), 3, np.float32)
        n = np.array([0, 0, -1], np.float32)
        if self.kind == "tilted":
            n = np.array([.2, .3, -1], np.float32)
            depth = (3 / (1 - .2*rays[..., 0] - .3*rays[..., 1])).astype(np.float32)
        if self.kind == "step":
            depth[:, w//2:] = 7
        normal = np.broadcast_to(n / np.linalg.norm(n), (h, w, 3)).copy()
        valid = np.ones((h, w), bool)
        if self.kind == "step":
            valid[8:15, 9:13] = False
        points = pinhole_points(depth, k)
        normal[~valid], points[~valid], depth[~valid] = 0, 0, 0
        result = GeometryPrediction(depth, normal, points, k, valid.astype(np.float32), valid, "synthetic",
            {"backend": self.name, "dummy": True, "confidence_kind": "binary synthetic fixture validity", "fixture": self.kind})
        validate_prediction(image, result)
        return result


def make_examples(root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=False)
    y, x = np.indices((99, 195))
    rgb = np.stack(((x*7)%256, (y*11)%256, ((x+y)*3)%256), -1).astype(np.uint8)
    source = root / "数值输入.png"
    Image.fromarray(rgb).save(source)
    for kind in ("plane", "tilted", "step"):
        ReconstructionPipeline(geometry_backend=SyntheticGeometry(kind)).run(source, root/kind, max_size=65)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    make_examples(parser.parse_args().output)
