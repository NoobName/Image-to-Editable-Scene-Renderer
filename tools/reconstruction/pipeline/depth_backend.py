"""Depth backends return metric camera-Z arrays, never normalized display images."""
from abc import ABC, abstractmethod
import math
import numpy as np
from .types import InputImage, FloatImage


class DepthBackend(ABC):
    name = "abstract"

    @abstractmethod
    def predict(self, image: InputImage) -> FloatImage:
        """Return H x W float32, positive camera Z in meters."""
        raise NotImplementedError


class DummyDepthBackend(DepthBackend):
    name = "dummy-flat-depth"

    def __init__(self, depth_meters: float = 3.0):
        if not math.isfinite(depth_meters) or not 0.1 <= depth_meters <= 100:
            raise ValueError("Dummy depth must be between 0.1 and 100 meters")
        self.depth_meters = depth_meters

    def predict(self, image: InputImage) -> FloatImage:
        return np.full((image.height, image.width), self.depth_meters, dtype=np.float32)
