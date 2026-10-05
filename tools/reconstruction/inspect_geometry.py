"""Numerically inspect exported geometry without importing a model or PyTorch."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image
from pipeline.geometry_backend import GeometryPrediction, validate_prediction, pinhole_points
from pipeline.image_io import load_image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--require-model", action="store_true")
    args = parser.parse_args()
    root = args.package
    image = load_image(root/"textures/base_color.png", max_size=2048)
    report = json.loads((root/"debug/reconstruction.json").read_text(encoding="utf-8"))
    with np.load(root/"debug/geometry.npz", allow_pickle=False) as data:
        result = GeometryPrediction(**{name:data[name] for name in data.files},
                                    scale_type=report["scale_type"], metadata=report["geometry_estimation"])
        validate_prediction(image, result)
        depth = result.depth[result.valid_mask]
        if args.require_model and (report["dummy"] or np.ptp(depth) < 1e-4):
            raise ValueError("Expected real model inference and non-flat geometry")
        for name in ("depth", "normal", "confidence"):
            with Image.open(root/f"debug/{name}.png") as preview:
                if preview.size != (image.width,image.height):
                    raise ValueError(f"Wrong {name} preview dimensions")
        projected = pinhole_points(result.depth,result.camera_intrinsics)
        error = float(np.abs(projected[result.valid_mask]-result.point_map[result.valid_mask]).max())
        # Current adapters promise projection-consistent points; future unconstrained adapters may differ.
        if error > max(1e-4,float(depth.max())*1e-4):
            raise ValueError(f"Point map and camera reprojection disagree: {error}")
        print(json.dumps({"backend":report["backends"]["geometry"],"device":result.metadata.get("device"),
                          "size":[image.width,image.height],"valid_fraction":float(result.valid_mask.mean()),
                          "depth_min_max":[float(depth.min()),float(depth.max())],
                          "depth_std":float(depth.std()),"point_projection_max_error":error,
                          "normal_mean":result.normal[result.valid_mask].mean(0).tolist(),
                          "geometry":report["geometry"]},indent=2))


if __name__ == "__main__":
    main()
