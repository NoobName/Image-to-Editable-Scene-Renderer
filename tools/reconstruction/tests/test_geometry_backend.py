"""Geometry contract and adapter tests require no ML framework, network or checkpoints."""
from dataclasses import replace
from pathlib import Path
import json
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
import numpy as np
from PIL import Image
from pipeline.geometry_backend import DummyGeometryBackend, GeometryEstimationBackend, pinhole_points, validate_prediction
from pipeline.adapters.moge import convert_output, MoGeGeometryBackend
from pipeline.image_io import load_image
from pipeline.geometry_builder import GeometryBuilder
from pipeline.material_backend import DummyMaterialBackend
from pipeline.types import AnalysisResult
from pipeline.runner import ReconstructionPipeline
from .support import make_input


class GeometryBackendTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(prefix="geometry-contract-")
        self.root = Path(self.temp.name)
        self.image = load_image(make_input(self.root / "input.png", (64, 48)))
        self.prediction = DummyGeometryBackend(fov_y_degrees=60).predict(self.image)

    def tearDown(self):
        self.temp.cleanup()

    def analysis(self, prediction):
        return AnalysisResult(prediction.depth, prediction.normal, np.ones(prediction.depth.shape, np.uint32),
                              DummyMaterialBackend().predict(self.image), prediction)

    def raw_output(self):
        points = self.prediction.point_map.copy()
        points[..., 1] *= -1
        normal = np.zeros_like(points)
        normal[:] = [0, .6, -.8]
        return {"depth": self.prediction.depth.copy(), "points": points, "normal": normal,
                "intrinsics": self.prediction.camera_intrinsics.copy(), "mask": self.prediction.valid_mask.copy()}

    def test_unified_dummy_contract(self):
        validate_prediction(self.image, self.prediction)
        self.assertEqual(self.prediction.scale_type, "synthetic")
        self.assertEqual(self.prediction.confidence.min(), 1)
        with self.assertRaises(TypeError):
            GeometryEstimationBackend()

    def test_opencv_conversion_and_invalid_sentinels(self):
        raw = self.raw_output()
        raw["mask"][0, 0] = False
        raw["points"][0, 0] = np.inf
        raw["depth"][0, 0] = np.inf
        raw["normal"][0, 1] = np.nan
        result = convert_output(self.image, raw, {})
        self.assertFalse(result.valid_mask[0, :2].any())
        np.testing.assert_array_equal(result.point_map[0, :2], 0)
        np.testing.assert_allclose(result.normal[1, 1], [0, -.6, -.8])
        np.testing.assert_allclose(result.point_map[1:, :], self.prediction.point_map[1:, :])
        self.assertEqual(result.confidence[0, 0], 0)
        self.assertEqual(result.metadata["confidence_kind"], "binary-validity-not-calibrated")
        self.assertTrue(np.isinf(raw["points"][0, 0]).all(), "adapter must not mutate caller arrays")

    def test_adapter_rejects_wrong_model_or_outputs(self):
        for field in ("normal", "depth", "intrinsics", "points", "mask"):
            raw = self.raw_output()
            del raw[field]
            with self.assertRaises(ValueError):
                convert_output(self.image, raw, {})
        raw = self.raw_output()
        raw["mask"][:] = False
        with self.assertRaisesRegex(ValueError, "no valid pixels"):
            convert_output(self.image, raw, {})
        raw = self.raw_output()
        raw["depth"] *= 2
        with self.assertRaisesRegex(ValueError, "disagree"):
            convert_output(self.image, raw, {})
        with self.assertRaises(ValueError):
            MoGeGeometryBackend(resolution_level=10)

    def test_contract_rejects_invalid_values(self):
        invalids = [replace(self.prediction, confidence=self.prediction.confidence*2),
                    replace(self.prediction, point_map=self.prediction.point_map.astype(np.float64)),
                    replace(self.prediction, normal=self.prediction.normal*2),
                    replace(self.prediction, camera_intrinsics=np.zeros((3,3), np.float32)),
                    replace(self.prediction, depth=self.prediction.depth*np.nan)]
        for result in invalids:
            with self.assertRaises(ValueError):
                validate_prediction(self.image, result)

    def test_mesh_consumes_points_and_estimated_camera(self):
        points = self.prediction.point_map.copy()
        points[..., 0] += .1  # Ensure builder uses point map, not silently reconstructed depth.
        result = replace(self.prediction, point_map=points)
        mesh = GeometryBuilder(grid_size=9, fov_y_degrees=30).build(self.image, self.analysis(result))
        self.assertAlmostEqual(mesh.fov_y_degrees, 60, places=4)
        x = (mesh.texcoords[:,0]*self.image.width).astype(int)
        y = (mesh.texcoords[:,1]*self.image.height).astype(int)
        np.testing.assert_allclose(mesh.positions, points[y,x])
        a,b,c = (mesh.positions[mesh.indices[:,i]] for i in range(3))
        self.assertTrue((np.cross(b-a,c-a)[:,2] < 0).all())

    def test_coarse_mesh_does_not_bridge_invalid_hole(self):
        mask = self.prediction.valid_mask.copy()
        mask[20:28, 28:36] = False  # No sampled grid corner need fall inside this hole.
        depth, normal, points = (v.copy() for v in (self.prediction.depth, self.prediction.normal, self.prediction.point_map))
        depth[~mask], normal[~mask], points[~mask] = 0,0,0
        result = replace(self.prediction, depth=depth, normal=normal, point_map=points,
                         valid_mask=mask, confidence=mask.astype(np.float32))
        with self.assertRaisesRegex(ValueError, "No valid triangles"):
            GeometryBuilder(grid_size=2).build(self.image, self.analysis(result))
        mesh = GeometryBuilder(grid_size=9).build(self.image, self.analysis(result))
        self.assertTrue(np.isfinite(mesh.positions).all())
        self.assertTrue((mesh.positions[:,2] > 0).all())

    def test_depth_discontinuity_and_confidence_filter(self):
        depth = self.prediction.depth.copy()
        depth[:,32:] = 20
        result = replace(self.prediction, depth=depth, point_map=pinhole_points(depth,self.prediction.camera_intrinsics))
        mesh = GeometryBuilder(grid_size=9).build(self.image, self.analysis(result))
        triangle_z = mesh.positions[mesh.indices,2]
        self.assertTrue((triangle_z.max(1) == triangle_z.min(1)).all())
        result = replace(self.prediction, confidence=self.prediction.confidence*.2)
        with self.assertRaisesRegex(ValueError, "No valid triangles"):
            GeometryBuilder().build(self.image, self.analysis(result))

    def test_unsupported_camera_and_scale_are_explicit(self):
        k = self.prediction.camera_intrinsics.copy()
        k[0,2] = .4
        for result in (replace(self.prediction,camera_intrinsics=k), replace(self.prediction,scale_type="relative")):
            with self.assertRaises(ValueError):
                GeometryBuilder().build(self.image,self.analysis(result))

    def test_unified_export_and_no_torch_import_on_dummy(self):
        class InjectedGeometry(GeometryEstimationBackend):
            name = "test-geometry"
            def predict(inner, image):
                return replace(self.prediction, scale_type="metric", metadata={"dummy":False, "confidence_kind":"test"})
        output = ReconstructionPipeline(geometry_backend=InjectedGeometry()).run(self.image.path,self.root/"scene",progress=lambda _:None)
        for name in ("depth.png", "normal.png", "confidence.png"):
            with Image.open(output/"debug"/name) as preview:
                self.assertEqual(preview.size,(64,48))
        with np.load(output/"debug/geometry.npz", allow_pickle=False) as data:
            self.assertEqual(set(data.files), {"depth","normal","point_map","camera_intrinsics","confidence","valid_mask"})
            np.testing.assert_array_equal(data["camera_intrinsics"], self.prediction.camera_intrinsics)
        report = json.loads((output/"debug/reconstruction.json").read_text())
        self.assertFalse(report["dummy"])
        self.assertEqual(report["backends"]["geometry"], "test-geometry")
        camera = json.loads((output/"debug/camera.json").read_text())
        self.assertAlmostEqual(camera["fov_y_degrees"],60,places=4)
        script = "import sys; from pipeline.runner import ReconstructionPipeline; assert 'torch' not in sys.modules"
        proc = subprocess.run([sys.executable,"-c",script],cwd=Path(__file__).resolve().parents[1],capture_output=True)
        self.assertEqual(proc.returncode,0,proc.stderr)
