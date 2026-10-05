"""AnalysisMaps v1: strict schema, bounded DDS, and cross-map coordinate invariants."""
from pathlib import Path
import numpy as np
try:
    from .package_schema import read_json, validate, _check_profile
    from .runtime_dds import read_dds
except ImportError:
    from package_schema import read_json, validate, _check_profile
    from runtime_dds import read_dds

SCHEMA = read_json(Path(__file__).resolve().parents[2] / "schemas/analysis-maps.schema.json")
_check_profile(SCHEMA)
SIDECAR = "analysis/analysis.json"
VECTOR = {"normal", "normalWorld", "position", "albedo", "tangentNormal"}
INTEGER = {"validity", "region"}


def load_analysis(root, appearance):
    if not (Path(root) / SIDECAR).exists():
        return None
    try:
        from .scene_package import asset_path
    except ImportError:
        from scene_package import asset_path
    try:
        data = read_json(asset_path(root, SIDECAR, "analysis", (".json",)))
        validate(data, SCHEMA, schema=SCHEMA)
        if appearance is None:
            raise ValueError("AnalysisMaps require a validated source anchor")
        if data["sourceId"] != appearance["sourceId"] or data["sourceSize"] != appearance["sourceImage"]["size"] or data["analysisSize"] != appearance["analysisImage"]["size"]:
            raise ValueError("Analysis/source identity or dimensions mismatch")
        if not np.allclose(data["sourceToAnalysis"], appearance["analysisMapping"]["sourceToAnalysis"], rtol=1e-6, atol=1e-7):
            raise ValueError("Analysis/source mapping mismatch")
        w, h = data["analysisSize"]
        if max(w, h) > 2048 or w*h > 4*1024*1024:
            raise ValueError("Analysis dimensions exceed runtime budget")
        k = np.array(data["camera"]["intrinsicsNormalized"]).reshape(3, 3)
        if k[0, 0] <= 0 or k[1, 1] <= 0 or not np.allclose(k[[0, 1, 2, 2, 2], [1, 0, 0, 1, 2]], [0, 0, 0, 0, 1]):
            raise ValueError("Invalid zero-skew pinhole intrinsics")
        if not np.allclose(data["camera"]["intrinsicsPixels"], (np.diag([w, h, 1]) @ k).reshape(-1), rtol=1e-6, atol=1e-7):
            raise ValueError("Pixel/normalized intrinsics mismatch")
        if data["camera"]["cameraToWorld"] != np.eye(4).reshape(-1).tolist():
            raise ValueError("Only the identity reconstruction world frame is supported")
        arrays, total = {}, 0
        geometry = {"depth", "normal", "normalWorld", "position", "geometryConfidence"}
        units = "meters" if data["camera"]["scaleType"] == "metric" else data["camera"]["scaleType"] + "-units"
        for key, record in data["maps"].items():
            if record["status"] != "available":
                continue
            expected = "R32_UINT" if key in INTEGER else "RGBA32_FLOAT" if key in VECTOR else "R32_FLOAT"
            space = "camera-z" if key == "depth" else "lh-camera" if key in ("normal", "position") else "lh-world" if key == "normalWorld" else "tangent" if key == "tangentNormal" else "linear-rgb" if key == "albedo" else "image"
            unit = units if key in ("depth", "position") else "unit-vector" if key in ("normal", "normalWorld", "tangentNormal") else "binary" if key == "validity" else "label-id" if key == "region" else "reflectance" if key == "albedo" else "score" if "Confidence" in key else "unitless"
            mask = "geometry" if key in geometry else "region-nonzero" if key == "regionConfidence" else "all-pixels"
            if (record["format"], record["space"], record["units"], record["validity"], record["sampling"]) != (expected, space, unit, mask, "nearest" if key in INTEGER else "guarded-bilinear"):
                raise ValueError(f"Incorrect semantic contract for {key}")
            path = asset_path(root, record["path"], "analysis", (".dds",))
            total += path.stat().st_size
            if total > 256 * 1024 * 1024:
                raise ValueError("Analysis maps exceed 256 MiB total")
            fmt, a = read_dds(path)
            if fmt != expected or record["size"] != [w, h] or list(a.shape[:2]) != [h, w]:
                raise ValueError(f"DDS/metadata dimensions or format mismatch: {key}")
            if key in VECTOR and np.any(a[..., 3] != 0):
                raise ValueError("RGBA padding must be zero")
            if not np.allclose(record["range"], [float(a.min()), float(a.max())], rtol=1e-5, atol=1e-6):
                raise ValueError(f"Map range mismatch: {key}")
            arrays[key] = a
        if any(key in arrays for key in geometry) and "validity" not in arrays:
            raise ValueError("Geometry maps require validity")
        if "validity" in arrays and not np.isin(arrays["validity"], [0, 1]).all():
            raise ValueError("Validity must contain only 0/1")
        if "regionConfidence" in arrays and "region" not in arrays:
            raise ValueError("Region confidence requires labels")
        valid = arrays.get("validity", np.zeros((h, w), np.uint32)) != 0
        for key, a in arrays.items():
            if key in geometry and np.any(a[~valid] != 0):
                raise ValueError("Invalid geometry must be finite zero")
            if key in ("normal", "normalWorld", "tangentNormal"):
                mask = valid if key != "tangentNormal" else np.ones_like(valid)
                if not np.allclose(np.linalg.norm(a[..., :3][mask], axis=-1), 1, atol=1e-4):
                    raise ValueError("Normals must be unit length on valid texels")
            if key in ("albedo", "roughness", "metallic") or "Confidence" in key:
                if np.any((a < 0) | (a > 1)):
                    raise ValueError("Material/quality values must be in [0,1]")
        if "depth" in arrays and np.any((arrays["depth"][valid] <= 0) | (arrays["depth"][valid] > 10000)):
            raise ValueError("Depth must be positive camera Z <=10000")
        if "normal" in arrays and "normalWorld" in arrays and not np.allclose(arrays["normal"], arrays["normalWorld"], atol=1e-6):
            raise ValueError("Camera/world normals disagree for identity pose")
        if "position" in arrays:
            if "depth" not in arrays:
                raise ValueError("Position requires depth")
            try:
                from .pipeline.geometry_backend import pinhole_points
            except ImportError:
                from pipeline.geometry_backend import pinhole_points
            expected = pinhole_points(arrays["depth"], k)
            if not np.allclose(arrays["position"][..., :3], expected, rtol=1e-4, atol=1e-4):
                raise ValueError("Position disagrees with camera Z / half-center LH projection")
        return data
    except (ValueError, OSError, KeyError) as error:
        raise ValueError(f"Invalid optional {SIDECAR}: {error}") from error
