"""Export numeric model outputs without decoding previews or modifying the source anchor."""
import numpy as np
try:
    from ..runtime_dds import write_dds
    from ..scene_package import write_json_atomic
except ImportError:
    from runtime_dds import write_dds
    from scene_package import write_json_atomic
from .material_estimation_backend import MaterialEstimate

KEYS = ("depth", "normal", "normalWorld", "position", "validity", "region", "albedo", "roughness", "metallic",
        "geometryConfidence", "materialConfidence", "regionConfidence", "tangentNormal")


def write_analysis_maps(root, image, analysis, appearance):
    prediction = analysis.geometry
    if prediction is None:
        return  # No calibrated/saved geometry contract; do not invent analysis camera data.
    directory = root / "analysis"
    directory.mkdir(exist_ok=True)
    h, w = prediction.depth.shape
    maps = {key: {"status": "unavailable", "reason": "No corresponding backend output was retained"} for key in KEYS}

    def add(key, array, space, units, validity, backend, meaning, kind, nearest=False):
        array = np.asarray(array)
        if array.shape[:2] != (h, w):
            raise ValueError(f"Analysis map {key} must match analysis resolution")
        if array.ndim == 3:
            # Fourth channel is padding, not a hidden confidence or position component.
            array = np.concatenate((array, np.zeros((h, w, 1), np.float32)), axis=-1)
        format_name = "R32_UINT" if array.dtype == np.uint32 else "RGBA32_FLOAT" if array.ndim == 3 else "R32_FLOAT"
        path = f"analysis/{key}.dds"
        write_dds(root / path, array, format_name)
        maps[key] = {"status": "available", "path": path, "format": format_name, "size": [w, h], "space": space,
            "units": units, "validity": validity, "sampling": "nearest" if nearest else "guarded-bilinear",
            "provenance": {"backend": str(backend), "meaning": str(meaning), "kind": kind},
            "range": [float(array.min()), float(array.max())]}

    meta = prediction.metadata
    backend = meta.get("backend", "retained-geometry")
    kind = "synthetic" if prediction.scale_type == "synthetic" else "estimated"
    units = "meters" if prediction.scale_type == "metric" else prediction.scale_type + "-units"
    add("depth", prediction.depth, "camera-z", units, "geometry", backend, "forward Z, not ray length", kind)
    add("position", prediction.point_map, "lh-camera", units, "geometry", backend, "observed camera XYZ", kind)
    add("normal", prediction.normal, "lh-camera", "unit-vector", "geometry", backend, "signed geometric normal", kind)
    add("normalWorld", prediction.normal, "lh-world", "unit-vector", "geometry", backend,
        "camera normal transformed by identity reconstruction cameraToWorld; unrelated to edited objects", "derived")
    add("validity", prediction.valid_mask.astype(np.uint32), "image", "binary", "all-pixels", backend, "authoritative geometry validity", "derived", True)
    add("geometryConfidence", prediction.confidence, "image", "score", "geometry", backend, meta.get("confidence_kind", "unspecified quality"), kind)
    segmentation_backend = analysis.segmentation.metadata.get("backend", "retained-segmentation") if analysis.segmentation is not None else "legacy-single-region"
    add("region", analysis.labels, "image", "label-id", "all-pixels", segmentation_backend, "0=unassigned; IDs preserved exactly", "derived", True)
    if analysis.segmentation is not None:
        score = np.zeros((h, w), np.float32)
        for region in analysis.segmentation.regions:
            score[region.mask] = region.score
        add("regionConfidence", score, "image", "score", "region-nonzero", segmentation_backend, "region proposal score; not calibrated semantic confidence", "derived")
    material = analysis.material
    if isinstance(material, MaterialEstimate):
        m = material.metadata
        for key, value, meaning in (("albedo", material.albedo, m["albedo_source"]),
                ("roughness", material.roughness, m.get("roughness_source", "unspecified")),
                ("metallic", material.metallic, m.get("metallic_source", "unspecified")),
                ("materialConfidence", material.confidence, m.get("confidence_semantics", "unspecified quality")),
                ("tangentNormal", material.normal, m["normal_source"])):
            fallback = key == "tangentNormal" and m["normal_source"] == "flat-tangent-fallback" or m["albedo_source"] == "neutral-fallback"
            add(key, value, "tangent" if key == "tangentNormal" else "linear-rgb" if key == "albedo" else "image",
                "unit-vector" if key == "tangentNormal" else "reflectance" if key == "albedo" else "score" if key == "materialConfidence" else "unitless",
                "all-pixels", m.get("backend", "retained-material"), meaning, "fallback" if fallback else "estimated")
    k = prediction.camera_intrinsics.astype(float)
    data = {"version": 1, "sourceId": appearance["sourceId"], "sourceSize": appearance["sourceImage"]["size"],
        "analysisSize": [w, h], "pixelConvention": "edge-origin-half-integer-centers",
        "sourceToAnalysis": appearance["analysisMapping"]["sourceToAnalysis"],
        "camera": {"intrinsicsNormalized": k.reshape(-1).tolist(), "intrinsicsPixels": (np.diag([w, h, 1]) @ k).reshape(-1).tolist(),
            "cameraToWorld": np.eye(4).reshape(-1).tolist(), "coordinateSystem": "left-handed-y-up",
            "worldFrame": "reconstruction-not-edited-scene", "scaleType": prediction.scale_type, "provenance": str(backend)},
        "sampling": {"relativeDepthThreshold": .15, "boundaryPolicy": "nearest-valid-center-or-all-valid-same-region-footprint"}, "maps": maps}
    write_json_atomic(directory / "analysis.json", data)
    return data
