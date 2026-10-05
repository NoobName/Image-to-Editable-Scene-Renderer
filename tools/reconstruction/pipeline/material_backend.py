"""Dummy material uses the source image as a placeholder base color, not intrinsic albedo."""
from abc import ABC, abstractmethod
from .types import InputImage, MaterialPrediction


class MaterialBackend(ABC):
    name = "abstract"

    @abstractmethod
    def predict(self, image: InputImage) -> MaterialPrediction:
        raise NotImplementedError


class DummyMaterialBackend(MaterialBackend):
    name = "dummy-image-material"

    def predict(self, image: InputImage) -> MaterialPrediction:
        return MaterialPrediction(base_color=image.rgb.copy(), roughness=0.8, metallic=0.0)
