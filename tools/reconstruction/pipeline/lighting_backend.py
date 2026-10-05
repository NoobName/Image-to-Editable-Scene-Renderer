"""Model-independent fixed-observation Lambert baseline; never produces relighted RGB."""
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
import numpy as np
from .material_estimation_backend import MaterialEstimate, srgb_to_linear

LUMA = np.array([.2126, .7152, .0722], np.float64)


@dataclass(frozen=True)
class LightingInput:
    rgb: np.ndarray             # analysis RGB8 sRGB, derived from the immutable source, never Scene/Look
    normal: np.ndarray          # signed unit LH camera XYZ (not tangent normal)
    albedo: np.ndarray          # fixed linear RGB reflectance
    valid: np.ndarray
    roughness: np.ndarray
    metallic: np.ndarray
    material_confidence: np.ndarray
    labels: np.ndarray
    excluded: np.ndarray        # known/heuristic emitter or caller exclusion mask
    albedo_source: str
    provenance: dict = field(default_factory=dict)


@dataclass(frozen=True)
class LightingEstimate:
    source: dict
    fit: dict
    proxy: np.ndarray
    old_shading: np.ndarray
    residual: np.ndarray
    fit_mask: np.ndarray
    weights: np.ndarray
    rejection_flags: np.ndarray


def make_lighting_input(image, analysis):
    shape = (image.height, image.width)
    material = analysis.material
    intrinsic = isinstance(material, MaterialEstimate)
    excluded = np.zeros(shape, bool)
    names = []
    if analysis.segmentation is not None:
        for region in analysis.segmentation.regions:
            # These are declared heuristics, not a new semantic model or a reliable emission detector.
            if any(word in region.name.casefold() for word in ("lamp", "light", "bulb", "sun", "screen", "television", "emissive")):
                excluded |= region.mask
                names.append(region.name)
    return LightingInput(image.rgb, analysis.normal, material.albedo if intrinsic else np.full((*shape, 3), .5, np.float32),
        analysis.geometry.valid_mask if analysis.geometry else np.ones(shape, bool),
        material.roughness if intrinsic else np.full(shape, .8, np.float32),
        material.metallic if intrinsic else np.zeros(shape, np.float32),
        material.confidence if intrinsic else np.zeros(shape, np.float32), analysis.labels, excluded,
        material.metadata["albedo_source"] if intrinsic else "legacy-photo-placeholder",
        {"materialBackend": material.metadata.get("backend", "unknown") if intrinsic else "legacy-placeholder",
         "excludedRegionNames": names, "normalSpace": "lh-camera"})


def light_record(towards_light, direct_rgb, ambient_rgb, provenance):
    def component(rgb):
        rgb = np.asarray(rgb, np.float64)
        intensity = float(np.max(rgb))
        return {"intensity": intensity, "color": (rgb/intensity if intensity > 0 else np.ones(3)).tolist()}
    return {"direction": (-np.asarray(towards_light)).tolist(), "direct": component(direct_rgb),
            "ambient": component(ambient_rgb), "provenance": provenance}


def coefficients(record):
    return tuple(np.array(record[key]["color"])*record[key]["intensity"] for key in ("direct", "ambient"))


class LightingEstimationBackend(ABC):
    name = "abstract-lighting"

    @abstractmethod
    def predict(self, observation: LightingInput) -> LightingEstimate:
        raise NotImplementedError


def prepare_proxy(observation):
    o = observation
    h, w = o.rgb.shape[:2]
    for name in ("valid", "roughness", "metallic", "material_confidence", "labels", "excluded"):
        if getattr(o, name).shape != (h, w):
            raise ValueError(f"Lighting {name} shape mismatch")
    if o.normal.shape != (h, w, 3) or o.albedo.shape != (h, w, 3) or o.rgb.dtype != np.uint8:
        raise ValueError("Lighting RGB/normal/albedo contract mismatch")
    if any(not np.isfinite(getattr(o, name)).all() for name in ("normal", "albedo", "roughness", "metallic", "material_confidence")):
        raise ValueError("Lighting input contains NaN/Inf")
    if np.any((o.albedo < 0) | (o.albedo > 1)):
        raise ValueError("Lighting albedo must be linear reflectance [0,1]")
    linear = srgb_to_linear(o.rgb.astype(np.float32)/255)
    tests = (("invalid-normal", ~o.valid | (np.abs(np.linalg.norm(o.normal, axis=-1)-1) > .005)),
             ("black-albedo", np.min(o.albedo, axis=-1) < .03),
             ("saturated-rgb", np.max(o.rgb, axis=-1) >= 250),
             ("dark-rgb", linear @ LUMA < .003),
             ("reflective-material", (o.metallic > .3) | (o.roughness < .2)),
             ("excluded-region", o.excluded))
    flags = np.zeros((h, w), np.uint32)
    for bit, (_, mask) in enumerate(tests):
        flags[mask] |= 1 << bit
    mask = flags == 0
    proxy = np.zeros((h, w, 3), np.float64)
    proxy[mask] = linear[mask]/o.albedo[mask]
    # One deterministic scalar from the fixed data. Neither exposure nor albedo gain is optimized.
    normalization = max(float(np.median(proxy[mask] @ LUMA)), 1e-6) if mask.any() else 1.
    proxy /= normalization
    weight = mask.astype(np.float64)*np.minimum(np.min(o.albedo, axis=-1)/.15, 1)*(.25+.75*np.clip(o.material_confidence, 0, 1))
    exclusions = {name: int(test.sum()) for name, test in tests}
    return proxy, weight, flags, normalization, exclusions


class ManualLightingBackend(LightingEstimationBackend):
    name = "manual-test"

    def __init__(self, direction=(.2, -.3, 1), direct=(1., 1., 1.), ambient=(.2, .2, .2)):
        direction = np.array(direction, np.float64)
        if direction.shape != (3,) or not np.isfinite(direction).all() or np.linalg.norm(direction) < 1e-6:
            raise ValueError("Manual light travel direction must be finite and nonzero")
        for value in (direct, ambient):
            if np.shape(value) != (3,) or not np.isfinite(value).all() or np.any(np.array(value) < 0) or np.any(np.array(value) > 64):
                raise ValueError("Manual relative RGB coefficients must lie in [0,64]")
        self.source = light_record(-direction/np.linalg.norm(direction), direct, ambient, "manual-test")

    def predict(self, observation):
        from .lighting_solver import finish_estimate
        proxy, weight, flags, normalization, exclusions = prepare_proxy(observation)
        return finish_estimate(observation, self.source, proxy, weight, flags, normalization, exclusions,
            {"backend": self.name, "status": "manual", "confidence": 0., "identifiable": False,
             "reasons": ["Explicit manual/test calibration; not an automatic estimate"], "normalEigenvalues": [0., 0., 0.], "directionGap": 0.})
