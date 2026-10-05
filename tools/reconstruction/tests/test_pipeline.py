import json
from pathlib import Path
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch
import numpy as np
import OpenEXR
from PIL import Image
from pipeline.runner import ReconstructionPipeline
from pipeline.depth_backend import DepthBackend
from pipeline.material_backend import DummyMaterialBackend
from scene_package import load_package
from .support import make_input

VALIDATORS = []
SCRIPT = Path(__file__).resolve().parents[1] / "reconstruct.py"


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(prefix="dummy-pipeline-")
        self.root = Path(self.temp.name)
        self.input = make_input(self.root / "输入 image.jpg", (160, 120))
        self.output = self.root / "移动场景 package"

    def tearDown(self):
        self.temp.cleanup()

    def run_pipeline(self, output=None, **kwargs):
        kwargs.setdefault("material_backend", DummyMaterialBackend())  # Explicit historical photo-placeholder coverage.
        return ReconstructionPipeline(**kwargs).run(self.input, output or self.output, progress=lambda text: None)

    def test_complete_package_exr_roundtrip_and_source_texture(self):
        root = self.run_pipeline()
        manifest = load_package(root)
        self.assertEqual(len(manifest["objects"]), 1)
        self.assertEqual(set(manifest["auxiliary"]), {"depth", "normal", "pointmap", "segmentation"})
        self.assertNotIn("material",manifest["objects"][0],"Keep the embedded GLB photo material")
        self.assertEqual(manifest["camera"]["aspect"], 160/120)
        self.assertLess(manifest["camera"]["near"],3)
        self.assertEqual(manifest["camera"]["far"],6)
        with OpenEXR.File(str(root / manifest["auxiliary"]["depth"])) as f:
            depth = f.channels()["Z"].pixels
            self.assertEqual(depth.dtype, np.float32)
            self.assertEqual(depth.shape, (120, 160))
            np.testing.assert_array_equal(depth, np.full_like(depth, 3))
        with OpenEXR.File(str(root / manifest["auxiliary"]["normal"])) as f:
            normals = f.channels()["RGB"].pixels
            self.assertTrue(np.all(normals == [0, 0, -1]))
        with OpenEXR.File(str(root / manifest["auxiliary"]["pointmap"])) as f:
            points = f.channels()["RGB"].pixels
            np.testing.assert_array_equal(points[..., 2], depth)
            self.assertLess(points[0, 0, 0], 0)
            self.assertGreater(points[0, 0, 1], 0)
            self.assertTrue(np.isfinite(points).all())
        with Image.open(root / "masks/segmentation.png") as mask:
            self.assertTrue(np.all(np.array(mask) == [1, 0, 0]))
        with Image.open(root / "textures/base_color.png") as texture, Image.open(self.input) as source:
            np.testing.assert_array_equal(np.array(texture), np.array(source))
        report = json.loads((root / "debug/reconstruction.json").read_text(encoding="utf-8"))
        self.assertTrue(report["dummy"])
        self.assertEqual(report["backends"]["geometry"], "dummy")
        self.assertEqual(report["backends"]["segmentation"], "dummy-single-object")
        self.assertEqual(report["backends"]["material"], "dummy-image-material")
        self.assertEqual(report["input"]["processed_size"], [160, 120])

    def test_glb_reflection_alignment_and_indices(self):
        root = self.run_pipeline()
        data = (root / "meshes/scene_mesh.glb").read_bytes()
        magic, version, length = struct.unpack_from("<III", data)
        self.assertEqual((magic, version, length), (0x46546C67, 2, len(data)))
        json_length, kind = struct.unpack_from("<II", data, 12)
        self.assertEqual(kind, 0x4E4F534A)
        self.assertEqual(json_length % 4, 0)
        document = json.loads(data[20:20+json_length])
        binary_length, binary_kind = struct.unpack_from("<II", data, 20+json_length)
        self.assertEqual(binary_kind, 0x004E4942)
        binary = data[28+json_length:]
        self.assertEqual(len(binary), binary_length)
        image_view = document["bufferViews"][document["images"][0]["bufferView"]]
        offset = image_view["byteOffset"]
        self.assertEqual(binary[offset:offset+image_view["byteLength"]], (root/"textures/base_color.png").read_bytes())
        self.assertEqual(document["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"], {"index":0})
        self.assertEqual(document["materials"][0]["pbrMetallicRoughness"]["metallicFactor"],0)
        self.assertEqual(document["samplers"][0]["wrapS"],33071)
        def values(index, dtype, components):
            accessor = document["accessors"][index]
            view = document["bufferViews"][accessor["bufferView"]]
            self.assertEqual(view["byteOffset"] % 4, 0)
            return np.frombuffer(binary, dtype=dtype, count=accessor["count"]*components,
                                 offset=view["byteOffset"]).reshape(-1, components)
        primitive = document["meshes"][0]["primitives"][0]
        positions = values(primitive["attributes"]["POSITION"], "<f4", 3)
        normals = values(primitive["attributes"]["NORMAL"], "<f4", 3)
        indices = values(primitive["indices"], "<u4", 1).reshape(-1, 3)
        self.assertTrue(np.all(positions[:, 2] == -3), "glTF stores RH -Z")
        self.assertTrue(np.all(normals == [0, 0, 1]))
        self.assertLess(indices.max(), len(positions))
        a, b, c = (positions[indices[:, i]] for i in range(3))
        self.assertTrue(np.all(np.cross(b-a, c-a)[:, 2] > 0), "RH winding must follow exported +Z normal")

    def test_cpp_readers(self):
        if not VALIDATORS:
            self.skipTest("Supply --validator to exercise C++ ScenePackageLoader")
        root = self.run_pipeline()
        expected = load_package(root)
        for validator in VALIDATORS:
            with self.subTest(validator=validator):
                result = subprocess.run([str(validator), str(root)], capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
                self.assertEqual(json.loads(result.stdout), expected)

    def test_backend_injection(self):
        class TestDepthBackend(DepthBackend):
            name = "test-depth-adapter"
            def predict(self, image):
                return np.full((image.height, image.width), 5, dtype=np.float32)
        root = self.run_pipeline(depth_backend=TestDepthBackend())
        with OpenEXR.File(str(root / "debug/depth.exr")) as f:
            self.assertTrue(np.all(f.channels()["Z"].pixels == 5))
        self.assertEqual(load_package(root)["camera"]["target"], [0, 0, 5])

    def test_cli_from_another_working_directory(self):
        result = subprocess.run([sys.executable, str(SCRIPT), self.input.name, "--output", "generated/scene01",
                                 "--grid-size", "9", "--max-size", "80", "--depth", "4"],
                                cwd=self.root, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
        root = self.root / "generated/scene01"
        self.assertTrue((root / "scene.json").exists())
        report = json.loads((root / "debug/reconstruction.json").read_text(encoding="utf-8"))
        self.assertEqual(report["input"]["processed_size"], [80, 60])
        self.assertLessEqual(report["geometry"]["vertices"], 81)
        self.assertEqual(report["geometry"]["depth_range_meters"], [4, 4])

    def test_nonempty_output_untouched(self):
        self.output.mkdir()
        sentinel = self.output / "keep.txt"
        sentinel.write_text("user data", encoding="utf-8")
        with self.assertRaises(FileExistsError):
            self.run_pipeline()
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "user data")
        self.assertEqual(list(self.output.iterdir()), [sentinel])

    def test_failed_export_leaves_no_partial_package(self):
        self.output.mkdir()  # An existing empty output must also survive failure.
        with patch("pipeline.scene_exporter.write_glb", side_effect=RuntimeError("simulated export failure")):
            with self.assertRaisesRegex(RuntimeError, "simulated"):
                self.run_pipeline()
        self.assertTrue(self.output.is_dir())
        self.assertEqual(list(self.output.iterdir()), [])
        self.assertFalse(list(self.root.glob(".*-staging-*")))

    def test_deterministic_package(self):
        first = self.run_pipeline()
        second = self.run_pipeline(output=self.root / "another")
        files = sorted(path.relative_to(first) for path in first.rglob("*") if path.is_file())
        self.assertEqual(files, sorted(path.relative_to(second) for path in second.rglob("*") if path.is_file()))
        for relative in files:
            self.assertEqual((first / relative).read_bytes(), (second / relative).read_bytes(), str(relative))

    def test_bad_input_creates_no_output(self):
        self.input.write_bytes(b"invalid JPEG")
        with self.assertRaises(OSError):
            self.run_pipeline()
        self.assertFalse(self.output.exists())
