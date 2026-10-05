"""Capture or propagate source evidence. Does not use editable scene.camera or a renderer."""
import copy
import hashlib
from pathlib import Path
from PIL import Image
try:
    from ..appearance_contract import load_extension, sha256_file, SIDECAR
    from ..scene_package import copy_asset, write_json_atomic
except ImportError:
    from appearance_contract import load_extension, sha256_file, SIDECAR
    from scene_package import copy_asset, write_json_atomic


def _record(root, relative, size):
    return {"path": relative, "sha256": sha256_file(root / relative), "size": list(size),
            "colorSpace": "srgb", "encoding": "rgb8"}


def write_appearance(root, image, prediction):
    previous = load_extension(image.anchor_package) if image.anchor_package else None
    if previous is not None:
        # Immutable source evidence survives remesh, segmentation and material estimation byte-for-byte.
        for record in (previous["sourceImage"], previous["analysisImage"], previous.get("originalFile")):
            if record:
                copy_asset(root, image.anchor_package / record["path"], record["path"])
        copy_asset(root, image.anchor_package / SIDECAR, SIDECAR)
        return previous
    processed = "textures/original_image.png"
    if not (root / processed).exists():
        Image.fromarray(image.rgb).save(root / processed)
    full = image.canonical_rgb is not None and image.original_bytes is not None and image.anchor_package is None
    anchor = "textures/source_anchor.png"
    if full:
        Image.fromarray(image.canonical_rgb).save(root / anchor)
    else:
        copy_asset(root, root / processed, anchor)
    sw, sh = (image.canonical_rgb.shape[1], image.canonical_rgb.shape[0]) if full else (image.width, image.height)
    aw, ah = image.width, image.height
    camera = {"status": "calibration-required", "reason": "Original source-camera contract unavailable; calibrate before image-space use."}
    if full and prediction is not None:
        k = prediction.camera_intrinsics.astype(float).reshape(-1).tolist()
        synthetic = prediction.scale_type == "synthetic"
        camera = {"status": "synthetic" if synthetic else "reconstruction-estimate", "coordinateSystem": "left-handed-y-up",
            "pixelConvention": "edge-origin-half-integer-centers", "intrinsicsNormalized": k,
            "sourceIntrinsicsPixels": [k[i] * (sw if i < 3 else sh if i < 6 else 1) for i in range(9)],
            "analysisIntrinsicsPixels": [k[i] * (aw if i < 3 else ah if i < 6 else 1) for i in range(9)],
            "cameraToWorld": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1],
            "backend": str(prediction.metadata.get("backend", "unspecified")), "scaleType": prediction.scale_type}
    data = {"version": 1, "sourceKind": "canonical" if full else "legacy-processed",
        "sourceImage": _record(root, anchor, (sw, sh)), "analysisImage": _record(root, processed, (aw, ah)),
        "normalization": copy.deepcopy(image.normalization) if full else {"pipeline": "legacy-processed-unknown", "precision": "rgb8-only"},
        "analysisMapping": {"pixelConvention": "edge-origin-half-integer-centers", "method": "full-frame-resize",
            "filter": "pillow-lanczos" if full else "legacy-unknown", "sourceToAnalysis": [aw/sw,0,0, 0,ah/sh,0, 0,0,1]},
        "sourceCamera": camera,
        "capabilities": {"fullResolutionAnchor": full, "sourceCameraAvailable": camera["status"] != "calibration-required",
            "requiresCalibration": camera["status"] != "reconstruction-estimate"}}
    if full:
        relative = "relighting/source/original" + (".png" if image.original_bytes.startswith(b"\x89PNG") else ".jpg")
        (root / relative).parent.mkdir(parents=True, exist_ok=True)
        (root / relative).write_bytes(image.original_bytes)
        data["originalFile"] = {"path": relative, "sha256": hashlib.sha256(image.original_bytes).hexdigest(), "storedSize": list(image.source_size)}
        data["sourceId"] = "original-sha256:" + data["originalFile"]["sha256"]
    else:
        data["sourceId"] = "legacy-processed-sha256:" + data["sourceImage"]["sha256"]
    write_json_atomic(root / SIDECAR, data)
    return data
