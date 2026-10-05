"""Depth/point-map geometry construction; independent of all model adapters."""
import math
import numpy as np
from .types import InputImage, AnalysisResult, GeometryResult, validate_analysis
from .geometry_backend import GeometryPrediction, pinhole_points
from .grid_triangulation import triangulate


class GeometryBuilder:
    def __init__(self, grid_size: int | None = None, fov_y_degrees: float = 45.0,
                 confidence_threshold: float = .5, depth_edge_threshold: float = .15,
                 *, depth_edge_meters: float = 0, max_edge_stretch: float = 8,
                 max_edge_meters: float = 0, max_triangle_area: float = 0):
        if grid_size is not None and (not isinstance(grid_size, int) or not 2 <= grid_size <= 257):
            raise ValueError("grid-size must be between 2 and 257; omit for one vertex per valid pixel")
        if not math.isfinite(fov_y_degrees) or not 10 <= fov_y_degrees <= 120:
            raise ValueError("fov must be between 10 and 120 degrees")
        if not 0 <= confidence_threshold <= 1 or not 0 < depth_edge_threshold <= 10:
            raise ValueError("confidence threshold must be in [0,1]; depth edge threshold in (0,10]")
        if not math.isfinite(max_edge_stretch) or max_edge_stretch < 1:
            raise ValueError("max-edge-stretch must be finite and >= 1")
        if any(not math.isfinite(v) or v < 0 for v in (depth_edge_meters, max_edge_meters, max_triangle_area)):
            raise ValueError("Absolute mesh limits must be finite and nonnegative; 0 disables a limit")
        self.grid_size, self.fov_y_degrees = grid_size, fov_y_degrees
        self.confidence_threshold, self.depth_edge_threshold = confidence_threshold, depth_edge_threshold
        self.depth_edge_meters, self.max_edge_stretch = depth_edge_meters, max_edge_stretch
        self.max_edge_meters, self.max_triangle_area = max_edge_meters, max_triangle_area

    def build(self, image: InputImage, analysis: AnalysisResult) -> GeometryResult:
        validate_analysis(image, analysis)
        prediction = analysis.geometry
        if prediction is None:
            # Depth-only backends need camera calibration. Legacy callers supply a known FOV.
            fy = .5/math.tan(math.radians(self.fov_y_degrees)/2)
            k = np.array([[fy*image.height/image.width,0,.5],[0,fy,.5],[0,0,1]], np.float32)
            prediction = GeometryPrediction(analysis.depth, analysis.normal, pinhole_points(analysis.depth,k), k,
                                            np.ones_like(analysis.depth), np.ones_like(analysis.depth,dtype=bool), "synthetic")
        k = prediction.camera_intrinsics
        if not np.allclose(k[:2,2], .5, atol=1e-5) or not np.isclose(k[0,0]*image.width,k[1,1]*image.height,rtol=1e-4):
            raise ValueError("ScenePackage v1 requires centered principal point and square pixels")
        if prediction.scale_type == "relative":
            raise ValueError("Relative geometry needs an explicit scale adapter before exporting meter-based ScenePackage")
        fov = math.degrees(2*math.atan(.5/float(k[1,1])))
        if not 1 <= fov <= 175:
            raise ValueError("Estimated FOV is outside ScenePackage range [1,175]")
        positions, normals, uv, indices, diagnostics = triangulate(
            prediction, grid_size=self.grid_size, confidence_threshold=self.confidence_threshold,
            depth_edge_threshold=self.depth_edge_threshold, depth_edge_meters=self.depth_edge_meters,
            max_edge_stretch=self.max_edge_stretch,
            max_edge_meters=self.max_edge_meters, max_triangle_area=self.max_triangle_area)
        return GeometryResult(positions, normals, uv, indices, prediction.point_map, fov, diagnostics)
