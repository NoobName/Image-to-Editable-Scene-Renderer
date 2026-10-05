"""Label IDs are integer data; visualization colors are exported separately."""
from abc import ABC, abstractmethod
import numpy as np
from .types import InputImage
from .segmentation_types import MaskProposal, SegmentationPrediction


class SegmentationBackend(ABC):
    name = "abstract"

    def release(self):
        """Optional GPU resource release between independent inference stages."""

    @abstractmethod
    def predict(self, image: InputImage) -> SegmentationPrediction:
        """Return CPU boolean mask proposals; model classes/tensors never escape adapters."""
        raise NotImplementedError


class DummySegmentationBackend(SegmentationBackend):
    name = "dummy-single-object"

    def predict(self, image: InputImage) -> SegmentationPrediction:
        return SegmentationPrediction((MaskProposal(np.ones((image.height,image.width),dtype=bool),
            object_id="image-surface",name="Image Surface",category="background"),), {"dummy":True})
