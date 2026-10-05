"""Analysis normals are camera-space data, not tangent-space material normal maps."""
from abc import ABC, abstractmethod
import numpy as np
from .types import InputImage, FloatImage


class NormalBackend(ABC):
    name = "abstract"

    @abstractmethod
    def predict(self, image: InputImage) -> FloatImage:
        """Return H x W x 3 float32, unit vectors in LH camera space."""
        raise NotImplementedError


class DummyNormalBackend(NormalBackend):
    name = "dummy-front-facing-normal"

    def predict(self, image: InputImage) -> FloatImage:
        normals = np.zeros((image.height, image.width, 3), dtype=np.float32)
        normals[..., 2] = -1  # Camera looks along +Z; the visible plane faces back toward it.
        return normals
