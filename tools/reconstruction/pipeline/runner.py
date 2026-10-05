"""Orchestration depends on backend interfaces; Dummy is a composition choice."""
from dataclasses import dataclass, field, replace
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
from .lighting_solver import RobustDirectionalAmbientBackend
from .lighting_backend import LightingEstimationBackend, make_lighting_input


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
    lighting_backend: LightingEstimationBackend = field(default_factory=RobustDirectionalAmbientBackend)

    def run(self, input_path: Path, output: Path, max_size: int = 1024, progress=print, stage_event=None) -> Path:
        event = stage_event or (lambda *args: None)
        output = check_destination(output)
        image = load_image(input_path, max_size)
        progress(f"Input: {image.width} x {image.height}, sRGB (oriented/resized)")
        # Stage-09 injected depth/normal backends still work through the unified Dummy adapter.
        backend = self.geometry_backend or DummyGeometryBackend(self.depth_backend, self.normal_backend,
                                                                 self.geometry_builder.fov_y_degrees)
        progress(f"geometry: {backend.name}")
        event("geometry", "running", backend.name)
        try:
            prediction = backend.predict(image)
        finally:
            backend.release()
        validate_prediction(image, prediction)
        event("geometry", "complete", backend.name)
        backends = {"segmentation": self.segmentation_backend, "material": self.material_backend}
        predictions = {}
        for name, stage in backends.items():
            stage_name = "materials" if name == "material" else name
            event(stage_name, "running", stage.name)
            progress(f"{name}: {stage.name}")
            try:
                predictions[name] = stage.predict(image)
            finally:
                if hasattr(stage, "release"):
                    stage.release()
            if name == "segmentation":
                segmentation = resolve_regions(image,predictions[name],prediction,
                                               min_pixels=self.min_region_pixels,max_regions=self.max_regions)
            else:
                validate_analysis(image, AnalysisResult(prediction.depth,prediction.normal,segmentation.labels,
                                                       predictions[name],prediction,segmentation))
            event(stage_name, "complete", stage.name)
        analysis = AnalysisResult(prediction.depth,prediction.normal,segmentation.labels,predictions["material"],prediction,segmentation)
        validate_analysis(image, analysis)
        event("lighting", "running", self.lighting_backend.name)
        from .material_estimation_backend import MaterialEstimate
        if isinstance(analysis.material, MaterialEstimate):
            analysis = replace(analysis, lighting=self.lighting_backend.predict(make_lighting_input(image, analysis)))
        event("lighting", "complete", analysis.lighting.fit["status"] if analysis.lighting else "Unavailable: legacy material has no estimated albedo")
        event("export", "running", "Building grid mesh and writing ScenePackage")
        geometry = self.geometry_builder.build(image, analysis)
        progress(f"Geometry: {len(geometry.positions)} vertices, {len(geometry.indices)} triangles")
        result = self.scene_exporter.export(output, image, analysis, geometry,
                                           {"geometry": backend.name, **{name: stage.name for name, stage in backends.items()}})
        event("export", "complete", "ScenePackage published")
        return result
