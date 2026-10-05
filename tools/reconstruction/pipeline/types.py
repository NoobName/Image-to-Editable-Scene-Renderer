"""Contracts between interchangeable inference stages and deterministic geometry/export."""
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING
import numpy as np
from numpy.typing import NDArray

FloatImage = NDArray[np.float32]
LabelImage = NDArray[np.uint32]
RGBImage = NDArray[np.uint8]
if TYPE_CHECKING:
    from .material_estimation_backend import MaterialEstimate
    from .geometry_backend import GeometryPrediction
    from .segmentation_types import SegmentationResult


@dataclass(frozen=True)
class InputImage:
    path: Path
    rgb: RGBImage  # H x W x 3, uint8, sRGB; top-left origin, read-only
    source_size: tuple[int, int]  # stored file width, height before EXIF orientation
    source_sha256: str
    color_profile_applied: bool = False
    # Models continue to consume rgb only. Full-resolution source ownership stays on CPU.
    canonical_rgb: RGBImage | None = None
    original_bytes: bytes | None = None
    normalization: dict = field(default_factory=dict)
    anchor_package: Path | None = None  # Re-export copies an existing anchor without re-encoding.

    @property
    def width(self) -> int:
        return self.rgb.shape[1]

    @property
    def height(self) -> int:
        return self.rgb.shape[0]


@dataclass(frozen=True)
class MaterialPrediction:
    base_color: RGBImage  # H x W x 3; placeholder copied from input, not estimated albedo
    roughness: float = 0.8
    metallic: float = 0.0


@dataclass(frozen=True)
class AnalysisResult:
    depth: FloatImage  # H x W, positive camera Z in meters (synthetic scale for Dummy)
    normal: FloatImage  # H x W x 3, unit vectors in LH camera space, +Y up, +Z forward
    labels: LabelImage  # H x W, RGB24-compatible region labels; 0 reserved for unassigned
    material: "MaterialPrediction | MaterialEstimate"
    geometry: "GeometryPrediction | None" = None
    segmentation: "SegmentationResult | None" = None
    lighting: object | None = None  # CPU-only LightingEstimate; no renderer/model coupling.


@dataclass(frozen=True)
class GeometryResult:
    positions: FloatImage  # N x 3, LH world space; camera at origin looking along +Z
    normals: FloatImage  # N x 3, same space
    texcoords: FloatImage  # N x 2, UV origin top-left
    indices: NDArray[np.uint32]  # T x 3; winding consistent with LH normals
    pointmap: FloatImage  # H x W x 3, pixel-center positions, same world space
    fov_y_degrees: float
    diagnostics: dict = field(default_factory=dict)


def validate_analysis(image: InputImage, result: AnalysisResult) -> None:
    shape = (image.height, image.width)
    valid = np.ones(shape, dtype=bool)
    if result.geometry is not None:
        from .geometry_backend import validate_prediction
        validate_prediction(image, result.geometry)
        valid = result.geometry.valid_mask
        if not np.array_equal(result.depth, result.geometry.depth) or not np.array_equal(result.normal, result.geometry.normal):
            raise ValueError("Analysis and unified geometry outputs disagree")
    for name, array, expected in (("depth", result.depth, shape),
                                  ("normal", result.normal, (*shape, 3))):
        if not isinstance(array, np.ndarray) or array.dtype != np.float32 or array.shape != expected:
            raise ValueError(f"{name} backend must return float32 with shape {expected}")
        if not np.isfinite(array).all():
            raise ValueError(f"{name} backend returned NaN or infinity")
    if np.any(result.depth[valid] <= 0) or np.any(result.depth[valid] > 10000):
        raise ValueError("Depth must be positive camera Z in (0, 10000] meters")
    lengths = np.linalg.norm(result.normal[valid], axis=-1)
    if not np.allclose(lengths, 1.0, atol=1e-4):
        raise ValueError("Normal backend must return unit vectors")
    if not isinstance(result.labels, np.ndarray) or result.labels.dtype != np.uint32 or result.labels.shape != shape:
        raise ValueError(f"Segmentation backend must return uint32 with shape {shape}")
    if not np.any(result.labels>0) or np.any(result.labels>0xFFFFFF):
        raise ValueError("Segmentation labels must include a region and fit RGB24")
    if result.segmentation is not None and not np.array_equal(result.labels,result.segmentation.labels):
        raise ValueError("Analysis and segmentation labels disagree")
    material = result.material
    from .material_estimation_backend import MaterialEstimate, validate_material
    if isinstance(material, MaterialEstimate):
        validate_material(image, material)
        return
    if not isinstance(material.base_color, np.ndarray) or material.base_color.dtype != np.uint8 or material.base_color.shape != (*shape, 3):
        raise ValueError("Material base_color must be uint8 RGB with the input dimensions")
    if not all(np.isfinite(v) and 0 <= v <= 1 for v in (material.roughness, material.metallic)):
        raise ValueError("Material roughness/metallic must be finite and in [0, 1]")
