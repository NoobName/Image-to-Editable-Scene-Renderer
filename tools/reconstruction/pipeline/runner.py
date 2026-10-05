"""Orchestration depends on backend interfaces; Dummy is a composition choice."""
from dataclasses import dataclass, field
from pathlib import Path
from .depth_backend import DepthBackend, DummyDepthBackend
from .normal_backend import NormalBackend, DummyNormalBackend
from .segmentation_backend import SegmentationBackend, DummySegmentationBackend
from .material_backend import MaterialBackend
from .material_estimation_backend import MaterialEstimationBackend, NeutralMaterialBackend
from .image_io import load_image
from .types import AnalysisResult, validate_analysis
from .geometry_builder import GeometryBuilder
from .scene_exporter import SceneExporter, check_destination
from .geometry_backend import GeometryEstimationBackend, DummyGeometryBackend, validate_prediction
from .scene_decomposer import resolve_regions


@dataclass
class ReconstructionPipeline:
    geometry_backend: GeometryEstimationBackend | None = None
    depth_backend: DepthBackend = field(default_factory=DummyDepthBackend)
    normal_backend: NormalBackend = field(default_factory=DummyNormalBackend)
    segmentation_backend: SegmentationBackend = field(default_factory=DummySegmentationBackend)
    material_backend: MaterialEstimationBackend | MaterialBackend = field(default_factory=NeutralMaterialBackend)
    geometry_builder: GeometryBuilder = field(default_factory=GeometryBuilder)
    scene_exporter: SceneExporter = field(default_factory=SceneExporter)
    min_region_pixels: int = 64
    max_regions: int = 32

    def run(self, input_path: Path, output: Path, max_size: int = 1024, progress=print) -> Path:
        output = check_destination(output)
        image = load_image(input_path, max_size)
        progress(f"Input: {image.width} x {image.height}, sRGB (oriented/resized)")
        # Stage-09 injected depth/normal backends still work through the unified Dummy adapter.
        backend = self.geometry_backend or DummyGeometryBackend(self.depth_backend, self.normal_backend,
                                                                 self.geometry_builder.fov_y_degrees)
        progress(f"geometry: {backend.name}")
        try:
            prediction = backend.predict(image)
        finally:
            backend.release()
        validate_prediction(image, prediction)
        backends = {"segmentation": self.segmentation_backend, "material": self.material_backend}
        predictions = {}
        for name, stage in backends.items():
            progress(f"{name}: {stage.name}")
            try:
                predictions[name] = stage.predict(image)
            finally:
                if hasattr(stage, "release"):
                    stage.release()
        segmentation = resolve_regions(image,predictions["segmentation"],prediction,
                                       min_pixels=self.min_region_pixels,max_regions=self.max_regions)
        analysis = AnalysisResult(prediction.depth,prediction.normal,segmentation.labels,predictions["material"],prediction,segmentation)
        validate_analysis(image, analysis)
        geometry = self.geometry_builder.build(image, analysis)
        progress(f"Geometry: {len(geometry.positions)} vertices, {len(geometry.indices)} triangles")
        return self.scene_exporter.export(output, image, analysis, geometry,
                                          {"geometry": backend.name, **{name: stage.name for name, stage in backends.items()}})
