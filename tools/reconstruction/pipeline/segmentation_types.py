"""CPU-only masks and object regions shared by all segmentation adapters."""
from dataclasses import dataclass, field
import numpy as np

CATEGORIES = ("sky", "ground", "foreground", "major", "background")


@dataclass(frozen=True)
class MaskProposal:
    mask: np.ndarray  # H x W bool; proposals may overlap
    score: float = 1.0  # Backend quality score, not semantic confidence
    object_id: str | None = None  # Explicit prompt ID, otherwise content-derived
    name: str | None = None
    category: str | None = None


@dataclass(frozen=True)
class SegmentationPrediction:
    proposals: tuple[MaskProposal, ...]
    metadata: dict = field(default_factory=dict)


@dataclass(frozen=True)
class ObjectRegion:
    object_id: str
    label_id: int  # Positive RGB24 label in this package
    name: str
    category: str
    mask: np.ndarray  # Exclusive resolved mask, not raw overlapping proposal
    bounding_box: tuple[int, int, int, int]  # Pixel x, y, width, height; half-open
    average_depth: float | None  # Mean valid camera Z; None when no geometry observation
    valid_depth_pixels: int
    naming_source: str
    score: float


@dataclass(frozen=True)
class SegmentationResult:
    labels: np.ndarray  # uint32 H x W, exclusive object labels
    regions: tuple[ObjectRegion, ...]
    metadata: dict = field(default_factory=dict)
