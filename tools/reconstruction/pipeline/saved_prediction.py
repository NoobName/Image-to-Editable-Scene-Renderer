"""Remesh saved stage-10 arrays without importing torch or running a model."""
from dataclasses import replace
import json
from pathlib import Path
import numpy as np
from .geometry_backend import GeometryPrediction, pinhole_points, validate_prediction
from .image_io import load_image
from .types import AnalysisResult, MaterialPrediction
from .scene_exporter import SceneExporter, check_destination


def load_saved_geometry(package: Path, geometry_source="point-map"):
    package = Path(package).resolve(strict=True)
    try:
        from ..scene_package import load_package
    except ImportError:
        from scene_package import load_package
    load_package(package)  # Validate optional provenance before deriving another package from it.
    if geometry_source not in ("point-map", "depth"):
        raise ValueError("Geometry source must be point-map or depth")
    report = json.loads((package/"debug/reconstruction.json").read_text(encoding="utf-8"))
    photo = package/"textures/original_image.png"
    image = load_image(photo if photo.is_file() else package/"textures/base_color.png", max_size=2048)
    with np.load(package/"debug/geometry.npz", allow_pickle=False) as saved:
        values = {name: saved[name] for name in ("depth", "normal", "point_map", "camera_intrinsics", "confidence", "valid_mask")}
    prediction = GeometryPrediction(**values, scale_type=report["scale_type"],
        metadata={**report["geometry_estimation"], "remesh_source": str(package), "mesh_position_source": geometry_source})
    validate_prediction(image, prediction)
    if geometry_source == "depth":
        prediction = replace(prediction, point_map=pinhole_points(prediction.depth, prediction.camera_intrinsics))
        validate_prediction(image, prediction)
    source = report["input"]
    if source["processed_size"] != [image.width,image.height]:
        raise ValueError("Saved image dimensions do not match the prediction report")
    image = replace(image, path=Path(source["name"]), source_size=tuple(source["source_size"]),
                    source_sha256=source["sha256"], color_profile_applied=source["icc_applied"],
                    canonical_rgb=None, original_bytes=None, normalization={}, anchor_package=package)
    return image,prediction,report


def remesh_saved(package: Path, output: Path, builder, geometry_source="point-map"):
    output = check_destination(output)
    image,prediction,report = load_saved_geometry(package,geometry_source)
    from .saved_segmentation import load_saved_segmentation
    segmentation = load_saved_segmentation(package,image,prediction)
    from .material_assets import load_saved_material
    analysis = AnalysisResult(prediction.depth, prediction.normal,
                              np.ones_like(prediction.depth,dtype=np.uint32) if segmentation is None else segmentation.labels,
                              load_saved_material(package,image),prediction,segmentation)
    mesh = builder.build(image, analysis)
    return SceneExporter().export(output,image,analysis,mesh,report["backends"])
