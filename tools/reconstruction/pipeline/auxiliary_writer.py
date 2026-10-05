"""Write real float32 EXR data and separate human-readable previews."""
from pathlib import Path
import json
import numpy as np
import OpenEXR
from PIL import Image, ImageDraw
from .types import AnalysisResult, GeometryResult


def write_exr(path: Path, pixels, channel: str, semantic: str):
    header = {"compression": OpenEXR.ZIP_COMPRESSION, "type": OpenEXR.scanlineimage,
              "isrCoordinateSystem": "left-handed-y-up", "isrSemantic": semantic}
    with OpenEXR.File(header, {channel: np.ascontiguousarray(pixels, dtype=np.float32)}) as file:
        file.write(str(path))


def write_auxiliary(root: Path, analysis: AnalysisResult, geometry: GeometryResult, original_rgb=None) -> dict:
    debug = root / "debug"
    prediction = analysis.geometry
    valid = prediction.valid_mask if prediction is not None else np.ones(analysis.depth.shape, dtype=bool)
    confidence = prediction.confidence if prediction is not None else valid.astype(np.float32)
    write_exr(debug / "depth.exr", analysis.depth, "Z", "camera-space forward Z; scale convention in reconstruction.json; zero invalid")
    write_exr(debug / "normal.exr", analysis.normal, "RGB", "camera-space signed XYZ unit normals; not color")
    write_exr(debug / "pointmap.exr", geometry.pointmap, "RGB", "world-space XYZ positions in meters; not color")
    labels = analysis.labels
    packed = np.stack((labels & 255, (labels >> 8) & 255, (labels >> 16) & 255), axis=-1).astype(np.uint8)
    Image.fromarray(packed).save(root / "masks/segmentation.png")
    normal_preview = np.rint(np.clip(analysis.normal*.5+.5, 0, 1)*255).astype(np.uint8)
    normal_preview[~valid] = 0
    Image.fromarray(normal_preview).save(debug / "normal_preview.png")
    Image.fromarray(normal_preview).save(debug / "normal.png")
    lo, hi = np.percentile(analysis.depth[valid], [2, 98])
    depth_preview = (np.full_like(analysis.depth, .5) if hi-lo < 1e-6 else np.clip((analysis.depth-lo)/(hi-lo), 0, 1))
    depth_preview[~valid] = 0
    for name in ("depth_preview.png", "depth.png"):
        Image.fromarray(np.rint(depth_preview*255).astype(np.uint8)).save(debug / name)
    Image.fromarray(np.rint(confidence*255).astype(np.uint8)).save(debug / "confidence.png")
    Image.fromarray(valid.astype(np.uint8)*255).save(debug / "valid_mask.png")
    write_exr(debug / "confidence.exr", confidence, "Y", "confidence semantics in reconstruction.json; not calibrated probability")
    if prediction is not None:
        np.savez_compressed(debug / "geometry.npz", depth=prediction.depth, normal=prediction.normal,
                            point_map=prediction.point_map, camera_intrinsics=prediction.camera_intrinsics,
                            confidence=prediction.confidence, valid_mask=prediction.valid_mask)
        camera = {"intrinsics_normalized": prediction.camera_intrinsics.tolist(),
                  "intrinsics_pixels": (np.diag([analysis.depth.shape[1], analysis.depth.shape[0], 1]) @ prediction.camera_intrinsics).tolist(),
                  "pixel_convention": "pixel centers at (column+0.5,row+0.5), image Y down",
                  "point_normal_coordinates": "LH camera: +X right, +Y up, +Z forward",
                  "scale_type": prediction.scale_type, "fov_y_degrees": geometry.fov_y_degrees,
                  "depth_preview_percentiles_2_98": [float(lo), float(hi)]}
        (debug / "camera.json").write_text(json.dumps(camera, indent=2, allow_nan=False)+"\n", encoding="utf-8")
    mask_preview = np.stack((40+(labels*73)%191,40+(labels*127)%191,40+(labels*167)%191),-1).astype(np.uint8)
    mask_preview[labels==0] = 0
    Image.fromarray(mask_preview).save(debug / "segmentation_preview.png")
    Image.fromarray(mask_preview).save(debug / "segmentation.png")
    overlay = Image.fromarray(np.rint((analysis.material.base_color if original_rgb is None else original_rgb)*.55+mask_preview*.45).astype(np.uint8))
    if analysis.segmentation is not None:
        drawing = ImageDraw.Draw(overlay)
        for region in analysis.segmentation.regions:
            x,y,w,h = region.bounding_box
            drawing.rectangle((x,y,x+w-1,y+h-1),outline="white",width=1)
            drawing.text((x+2,y+2),str(region.label_id),fill="white",stroke_width=1,stroke_fill="black")
    overlay.save(debug/"segmentation_overlay.png")
    return {"depth": "debug/depth.exr", "normal": "debug/normal.exr",
            "pointmap": "debug/pointmap.exr", "segmentation": "masks/segmentation.png"}
