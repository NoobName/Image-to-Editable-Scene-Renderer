"""Model-independent PBR maps. Albedo is linear reflectance, not the illuminated RGB input."""
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
import numpy as np


def srgb_to_linear(value):
    value = np.asarray(value, dtype=np.float32)
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4).astype(np.float32)


def linear_to_srgb(value):
    value = np.clip(value, 0, 1)
    return np.where(value <= .0031308, value * 12.92, 1.055 * value ** (1 / 2.4) - .055)


def quantize(value):
    return np.rint(np.clip(value, 0, 1) * 255).astype(np.uint8)


@dataclass(frozen=True)
class MaterialEstimate:
    albedo: np.ndarray       # H,W,3 float32 LINEAR reflectance [0,1]
    roughness: np.ndarray    # H,W float32 perceptual roughness [0,1], not squared alpha
    metallic: np.ndarray     # H,W float32 [0,1]
    normal: np.ndarray       # H,W,3 float32 unit tangent XYZ; +Z is unperturbed surface
    confidence: np.ndarray   # H,W float32 [0,1]; exact meaning in metadata, never assumed calibrated
    metadata: dict = field(default_factory=dict)

    @property
    def base_color(self):
        return quantize(linear_to_srgb(self.albedo))


def validate_material(image, value):
    h, w = image.height, image.width
    for name, shape in (("albedo", (h, w, 3)), ("roughness", (h, w)), ("metallic", (h, w)),
                        ("normal", (h, w, 3)), ("confidence", (h, w))):
        array = getattr(value, name)
        if not isinstance(array, np.ndarray) or array.dtype != np.float32 or array.shape != shape:
            raise ValueError(f"Material {name} must be float32 {shape}")
        if not np.isfinite(array).all():
            raise ValueError(f"Material {name} contains NaN or infinity")
        if name != "normal" and ((array < 0).any() or (array > 1).any()):
            raise ValueError(f"Material {name} must be in [0,1]")
    if not np.allclose(np.linalg.norm(value.normal, axis=2), 1, atol=1e-4):
        raise ValueError("Material normals must be unit tangent vectors")
    if value.metadata.get("normal_space") != "tangent":
        raise ValueError("Camera/world normals cannot be bound as tangent material normal maps")
    if value.metadata.get("albedo_source") not in ("intrinsic", "neutral-fallback"):
        raise ValueError("Estimated albedo must identify intrinsic estimation or neutral fallback")
    if value.metadata.get("normal_source") not in ("estimated-tangent", "flat-tangent-fallback"):
        raise ValueError("Unknown material normal source")


class MaterialEstimationBackend(ABC):
    name = "abstract-material-estimation"

    @abstractmethod
    def predict(self, image) -> MaterialEstimate:
        raise NotImplementedError

    def release(self):
        """Optional inference-resource release; predictions must already be CPU-owned arrays."""


class NeutralMaterialBackend(MaterialEstimationBackend):
    name = "neutral-material"

    def predict(self, image):
        h, w = image.height, image.width
        normal = np.zeros((h, w, 3), np.float32)
        normal[:, :, 2] = 1
        return MaterialEstimate(np.full((h, w, 3), .5, np.float32), np.full((h, w), .8, np.float32),
            np.zeros((h, w), np.float32), normal, np.zeros((h, w), np.float32),
            {"backend": self.name, "albedo_source": "neutral-fallback", "normal_source": "flat-tangent-fallback",
             "normal_space": "tangent", "roughness_source": "default-0.8", "metallic_source": "default-0",
             "confidence_semantics": "zero: no material inference performed", "normal_confidence": 0,
             "fallbacks": ["albedo", "roughness", "metallic", "normal"], "dummy": True})
