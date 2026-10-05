"""Model-independent, CPU-only geometry contract. No ML framework imports here."""
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
import math
import numpy as np
from .types import InputImage
from .depth_backend import DepthBackend, DummyDepthBackend
from .normal_backend import NormalBackend, DummyNormalBackend


@dataclass(frozen=True)
class GeometryPrediction:
    depth: np.ndarray                 # float32 H,W: camera Z, zero when invalid
    normal: np.ndarray                # float32 H,W,3: LH camera XYZ, unit on valid pixels
    point_map: np.ndarray             # float32 H,W,3: LH camera XYZ, zero when invalid
    camera_intrinsics: np.ndarray     # float32 3,3: normalized UV, image Y down
    confidence: np.ndarray            # float32 H,W in [0,1]; meaning in metadata
    valid_mask: np.ndarray            # bool H,W; always authoritative
    scale_type: str = "metric"        # metric / relative / synthetic
    metadata: dict = field(default_factory=dict)


class GeometryEstimationBackend(ABC):
    name = "geometry-backend"

    def release(self):
        """Release optional inference resources after CPU-owned outputs have been produced."""

    @abstractmethod
    def predict(self, image: InputImage) -> GeometryPrediction:
        raise NotImplementedError


def pinhole_points(depth, intrinsics, uv=None):
    h, w = depth.shape
    if uv is None:
        u, v = np.meshgrid((np.arange(w, dtype=np.float32)+.5)/w,
                           (np.arange(h, dtype=np.float32)+.5)/h)
    else:
        u, v = uv[..., 0], uv[..., 1]
    k = intrinsics
    return np.stack(((u-k[0, 2])*depth/k[0, 0], -(v-k[1, 2])*depth/k[1, 1], depth), -1).astype(np.float32)


def validate_prediction(image, result):
    h, w = image.height, image.width
    for name, shape in (("depth", (h, w)), ("normal", (h, w, 3)), ("point_map", (h, w, 3)),
                        ("confidence", (h, w)), ("camera_intrinsics", (3, 3))):
        array = getattr(result, name)
        if not isinstance(array, np.ndarray) or array.dtype != np.float32 or array.shape != shape:
            raise ValueError(f"Geometry {name} must be float32 with shape {shape}")
        if not np.isfinite(array).all():
            raise ValueError(f"Geometry {name} must be finite, including invalid pixels")
    mask = result.valid_mask
    if not isinstance(mask, np.ndarray) or mask.dtype != np.bool_ or mask.shape != (h, w):
        raise ValueError("Geometry valid_mask must be bool H x W")
    if not mask.any():
        raise ValueError("Geometry prediction contains no valid pixels")
    if np.any((result.confidence < 0) | (result.confidence > 1)):
        raise ValueError("Geometry confidence must be in [0,1]")
    if np.any(result.confidence[~mask] != 0):
        raise ValueError("Invalid pixels must have zero confidence")
    if np.any(result.depth[mask] <= 0) or np.any(result.depth[mask] > 10000):
        raise ValueError("Valid camera Z must be in (0,10000]")
    if not np.allclose(np.linalg.norm(result.normal[mask], axis=-1), 1, atol=1e-4):
        raise ValueError("Valid normals must have unit length")
    if not np.allclose(result.point_map[..., 2][mask], result.depth[mask], rtol=1e-4, atol=1e-5):
        raise ValueError("Point-map Z and depth disagree")
    for array in (result.depth, result.normal, result.point_map):
        if np.any(array[~mask] != 0):
            raise ValueError("Invalid geometry must use finite zero sentinels")
    k = result.camera_intrinsics
    if k[0, 0] <= 0 or k[1, 1] <= 0 or not np.allclose(k[2], [0, 0, 1]) or k[0, 1] != 0 or k[1, 0] != 0:
        raise ValueError("Expected positive focal lengths and a zero-skew pinhole intrinsics matrix")
    if result.scale_type not in ("metric", "relative", "synthetic"):
        raise ValueError("Unknown geometry scale convention")


class DummyGeometryBackend(GeometryEstimationBackend):
    name = "dummy"

    def __init__(self, depth_backend: DepthBackend = None, normal_backend: NormalBackend = None, fov_y_degrees=45.):
        if not math.isfinite(fov_y_degrees) or not 10 <= fov_y_degrees <= 120:
            raise ValueError("Dummy FOV must be in [10,120] degrees")
        self.depth_backend = depth_backend or DummyDepthBackend()
        self.normal_backend = normal_backend or DummyNormalBackend()
        self.fov_y_degrees = fov_y_degrees

    def predict(self, image):
        depth, normal = self.depth_backend.predict(image), self.normal_backend.predict(image)
        fy = .5/math.tan(math.radians(self.fov_y_degrees)/2)
        k = np.array([[fy*image.height/image.width, 0, .5], [0, fy, .5], [0, 0, 1]], np.float32)
        result = GeometryPrediction(depth, normal, pinhole_points(depth, k), k,
                                    np.ones_like(depth), np.ones(depth.shape, dtype=bool), "synthetic",
                                    {"backend": self.name, "dummy": True, "confidence_kind": "synthetic-validity"})
        validate_prediction(image, result)
        return result
