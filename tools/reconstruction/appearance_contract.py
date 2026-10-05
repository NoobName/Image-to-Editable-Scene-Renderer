"""Optional v1 sidecar: strict shared schema plus cross-field/file invariants, no model imports."""
import hashlib
import math
from pathlib import Path
import re

try:
    from .package_schema import read_json, validate, _check_profile
except ImportError:
    from package_schema import read_json, validate, _check_profile

SIDECAR = "relighting/relighting.json"
SCHEMA = read_json(Path(__file__).resolve().parents[2] / "schemas/relighting.schema.json")
_check_profile(SCHEMA)
MAX_PIXELS = 40_000_000
MAX_BYTES = 128 * 1024 * 1024


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _require(ok, reason):
    if not ok:
        raise ValueError("AppearanceAnchor: " + reason)


def _close(actual, expected, name):
    _require(len(actual) == len(expected) and all(math.isclose(a, b, rel_tol=1e-6, abs_tol=1e-7)
             for a, b in zip(actual, expected)), f"{name} does not match the image coordinate contract")


def _image(root, record, original=False):
    try:
        from .scene_package import asset_path
    except ImportError:
        from scene_package import asset_path
    from PIL import Image  # Optional extension only; legacy v1 validation remains stdlib-only.
    size = record["storedSize" if original else "size"]
    _require(size[0] * size[1] <= MAX_PIXELS, "image exceeds 40 megapixels")
    _require(bool(re.fullmatch("[0-9a-f]{64}", record["sha256"])), "invalid lowercase SHA-256")
    path = asset_path(root, record["path"], "relighting" if original else "textures",
                      (".jpg", ".jpeg", ".png") if original else (".png",))
    _require(path.stat().st_size <= MAX_BYTES, "image exceeds 128 MiB")
    _require(sha256_file(path) == record["sha256"], "SHA-256 mismatch: " + record["path"])
    with Image.open(path) as image:
        _require(list(image.size) == size, "image dimensions mismatch: " + record["path"])
        _require(image.format in ("JPEG", "PNG") and getattr(image, "n_frames", 1) == 1, "unsupported image format")
        if not original:
            _require(image.mode == "RGB" and image.format == "PNG", "anchor/analysis must be RGB8 PNG")
            _require("icc_profile" not in image.info and "exif" not in image.info, "normalized PNG must not contain ICC/EXIF")
        image.load()  # Validate actual bounded pixel payload, not just a convenient file name.
    return path


def validate_extension(root, data):
    validate(data, SCHEMA, "relighting", SCHEMA)
    source, analysis = data["sourceImage"], data["analysisImage"]
    _image(root, source)
    _image(root, analysis)
    _require(source["path"] != analysis["path"] and analysis["path"] == "textures/original_image.png",
             "source anchor must be separate from processed original_image.png")
    sw, sh = source["size"]
    aw, ah = analysis["size"]
    _require(max(aw, ah) <= 2048 and aw <= sw and ah <= sh, "invalid analysis dimensions")
    _require(.01 <= aw / ah <= 100, "analysis aspect outside v1 limits")
    mapping = data["analysisMapping"]
    _close(mapping["sourceToAnalysis"], [aw/sw,0,0, 0,ah/sh,0, 0,0,1], "sourceToAnalysis")
    norm = data["normalization"]
    full = data["sourceKind"] == "canonical"
    if full:
        _require("originalFile" in data and norm["pipeline"] == "pillow-exif-icc-white-rgb8-v1", "canonical provenance is incomplete")
        _image(root, data["originalFile"], original=True)
        w, h = data["originalFile"]["storedSize"]
        orientation = norm["exifOrientation"]
        from_orientation = [h, w] if orientation >= 5 else [w, h]
        _require(source["size"] == from_orientation, "canonical dimensions disagree with EXIF orientation")
        # Small pure coordinate helper; does not import torch or run normalization again.
        try:
            from .pipeline.image_io import orientation_matrix
        except ImportError:
            from pipeline.image_io import orientation_matrix
        _close(norm["storedToCanonical"], orientation_matrix(orientation, w, h), "storedToCanonical")
        expected_id = "original-sha256:" + data["originalFile"]["sha256"]
        _require(mapping["filter"] == "pillow-lanczos", "canonical resize filter is missing")
    else:
        _require("originalFile" not in data and norm["pipeline"] == "legacy-processed-unknown", "legacy anchor cannot claim original provenance")
        _require(source["size"] == analysis["size"] and mapping["filter"] == "legacy-unknown", "legacy anchor must keep processed dimensions")
        _require(source["sha256"] == analysis["sha256"], "legacy anchor must preserve processed image bytes")
        expected_id = "legacy-processed-sha256:" + source["sha256"]
    _require(data["sourceId"] == expected_id, "sourceId disagrees with source provenance")
    camera = data["sourceCamera"]
    available = camera["status"] != "calibration-required"
    _require(full or not available, "legacy package needs source-camera calibration")
    if available:
        k = camera["intrinsicsNormalized"]
        _require(k[0] > 0 and k[4] > 0, "focal lengths must be positive")
        _close([k[i] for i in (1,3,6,7,8)], [0,0,0,0,1], "zero-skew intrinsics")
        _close(camera["sourceIntrinsicsPixels"], [k[i] * (sw if i < 3 else sh if i < 6 else 1) for i in range(9)], "source intrinsics")
        _close(camera["analysisIntrinsicsPixels"], [k[i] * (aw if i < 3 else ah if i < 6 else 1) for i in range(9)], "analysis intrinsics")
        _close(camera["cameraToWorld"], [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1], "reconstruction camera pose")
        _require((camera["status"] == "synthetic") == (camera["scaleType"] == "synthetic"), "camera scale/provenance disagree")
    _require(data["capabilities"] == {"fullResolutionAnchor": full, "sourceCameraAvailable": available,
             "requiresCalibration": not available or camera["status"] == "synthetic"}, "capabilities disagree with evidence")
    return data


def load_extension(root):
    try:
        from .scene_package import asset_path
    except ImportError:
        from scene_package import asset_path
    path = Path(root) / SIDECAR
    # A dangling symlink or a directory at the sidecar path is damage, not absence.
    if not path.exists() and not path.is_symlink():
        return None
    try:
        path = asset_path(root, SIDECAR, "relighting", (".json",))
        return validate_extension(root, read_json(path))
    except (ValueError, OSError, KeyError) as error:
        raise ValueError(f"Invalid optional {SIDECAR}: {error}") from error
