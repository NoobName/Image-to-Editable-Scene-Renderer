from dataclasses import replace
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch
import numpy as np
from PIL import Image, ImageCms
from pipeline.image_io import load_image
from pipeline.depth_backend import DepthBackend, DummyDepthBackend
from pipeline.normal_backend import DummyNormalBackend
from pipeline.segmentation_backend import DummySegmentationBackend
from pipeline.material_backend import DummyMaterialBackend
from pipeline.geometry_builder import GeometryBuilder
from pipeline.geometry_backend import DummyGeometryBackend
from pipeline.scene_decomposer import resolve_regions
from pipeline.types import AnalysisResult, validate_analysis
from .support import make_input


class StageTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(prefix="reconstruction-stages-")
        self.root = Path(self.temp.name)
        self.image = load_image(make_input(self.root / "input.png"))
        self.analysis = AnalysisResult(DummyDepthBackend().predict(self.image), DummyNormalBackend().predict(self.image),
                                       resolve_regions(self.image,DummySegmentationBackend().predict(self.image),DummyGeometryBackend().predict(self.image)).labels,
                                       DummyMaterialBackend().predict(self.image))

    def tearDown(self):
        self.temp.cleanup()

    def test_backend_contracts_and_readonly_input(self):
        validate_analysis(self.image, self.analysis)
        self.assertTrue(np.all(self.analysis.depth == 3))
        self.assertTrue(np.all(self.analysis.normal == [0, 0, -1]))
        self.assertTrue(np.all(self.analysis.labels == 1))
        np.testing.assert_array_equal(self.image.rgb, self.analysis.material.base_color)
        self.analysis.material.base_color[0, 0] = 0
        self.assertFalse(np.array_equal(self.image.rgb[0, 0], [0, 0, 0]))
        with self.assertRaises(ValueError):
            self.image.rgb[0, 0] = 0
        with self.assertRaises(TypeError):
            DepthBackend()

    def test_resize_grayscale_and_jpeg(self):
        path = self.root / "gray.jpg"
        Image.new("L", (600, 300), 128).save(path)
        image = load_image(path, max_size=100)
        self.assertEqual(image.rgb.shape, (50, 100, 3))
        np.testing.assert_array_equal(image.rgb[..., 0], image.rgb[..., 1])
        self.assertEqual(image.source_size, (600, 300))

    def test_exif_orientation_before_analysis(self):
        # A 3x2 image rotated 90 degrees clockwise becomes 2x3.
        source = Image.new("RGB", (3, 2), "white")
        source.putpixel((0, 0), (255, 0, 0))
        source.putpixel((2, 1), (0, 0, 255))
        exif = Image.Exif()
        exif[274] = 6
        path = self.root / "oriented.png"
        source.save(path, exif=exif)
        image = load_image(path)
        self.assertEqual(image.rgb.shape, (3, 2, 3))
        np.testing.assert_array_equal(image.rgb[0, 1], [255, 0, 0])
        np.testing.assert_array_equal(image.rgb[2, 0], [0, 0, 255])

    def test_alpha_and_icc(self):
        source = Image.new("RGBA", (2, 2), (255, 0, 0, 0))
        source.putpixel((1, 1), (10, 20, 30, 255))
        path = self.root / "transparent.png"
        profile = ImageCms.ImageCmsProfile(ImageCms.createProfile("sRGB")).tobytes()
        source.save(path, icc_profile=profile)
        image = load_image(path)
        self.assertTrue(image.color_profile_applied)
        np.testing.assert_array_equal(image.rgb[0, 0], [255, 255, 255])
        np.testing.assert_array_equal(image.rgb[1, 1], [10, 20, 30])

    def test_invalid_input_and_configuration(self):
        bad = self.root / "bad.jpg"
        bad.write_bytes(b"not an image")
        with self.assertRaises(OSError):
            load_image(bad)
        with self.assertRaises(FileNotFoundError):
            load_image(self.root / "missing.png")
        for depth in (0, -1, float("nan"), float("inf"), 101):
            with self.assertRaises(ValueError):
                DummyDepthBackend(depth)
        for grid in (1, 258):
            with self.assertRaises(ValueError):
                GeometryBuilder(grid_size=grid)
        for fov in (0, 180, float("nan")):
            with self.assertRaises(ValueError):
                GeometryBuilder(fov_y_degrees=fov)
        for size in (15, 2049):
            with self.assertRaises(ValueError):
                load_image(self.image.path, max_size=size)
        Image.new("RGB", (2, 2)).save(self.root / "input.gif")
        with self.assertRaises(ValueError):
            load_image(self.root / "input.gif")

    def test_rejects_invalid_backend_results(self):
        for depth in (self.analysis.depth.astype(np.float64), self.analysis.depth[:-1],
                      np.full_like(self.analysis.depth, np.nan), -self.analysis.depth):
            with self.assertRaises(ValueError):
                validate_analysis(self.image, replace(self.analysis, depth=depth))
        with self.assertRaises(ValueError):
            validate_analysis(self.image, replace(self.analysis, normal=self.analysis.normal*2))
        with self.assertRaises(ValueError):
            validate_analysis(self.image, replace(self.analysis, labels=np.zeros_like(self.analysis.labels)))
        with self.assertRaises(ValueError):
            validate_analysis(self.image, replace(self.analysis, material=replace(self.analysis.material, roughness=2)))

    def test_decoder_limit_errors_are_actionable(self):
        # Reduce Pillow's header limit; no large allocation is needed to exercise this path.
        with patch.object(Image, "MAX_IMAGE_PIXELS", 10):
            with self.assertRaisesRegex(ValueError, "Cannot decode input image"):
                load_image(self.image.path)

    def test_backprojection_pixel_centers_and_winding(self):
        geometry = GeometryBuilder(grid_size=17).build(self.image, self.analysis)
        pointmap = geometry.pointmap
        self.assertEqual(pointmap.shape, self.analysis.normal.shape)
        np.testing.assert_array_equal(pointmap[..., 2], self.analysis.depth)
        tan_half_fov = np.tan(np.deg2rad(geometry.fov_y_degrees)/2)
        projected_u = (pointmap[..., 0]/(pointmap[..., 2]*tan_half_fov*(self.image.width/self.image.height))+1)/2
        projected_v = (1-pointmap[..., 1]/(pointmap[..., 2]*tan_half_fov))/2
        np.testing.assert_allclose(projected_u[0], (np.arange(self.image.width)+.5)/self.image.width, atol=1e-7)
        np.testing.assert_allclose(projected_v[:, 0], (np.arange(self.image.height)+.5)/self.image.height, atol=1e-7)
        self.assertLess(pointmap[0, 0, 0], 0)
        self.assertGreater(pointmap[0, 0, 1], 0)
        self.assertLessEqual(len(geometry.positions), 17*17)
        self.assertLess(geometry.indices.max(), len(geometry.positions))
        a, b, c = (geometry.positions[geometry.indices[:, i]] for i in range(3))
        cross = np.cross(b-a, c-a)
        self.assertTrue(np.all(cross[:, 2] < 0), "LH winding must face the camera (-Z)")
        np.testing.assert_allclose(geometry.texcoords[0], [.5/self.image.width, .5/self.image.height])
        np.testing.assert_allclose(geometry.texcoords[-1], [1-.5/self.image.width, 1-.5/self.image.height])

    def test_single_pixel_input_cannot_define_a_triangle(self):
        path = self.root / "one.png"
        Image.new("RGB", (1, 1), "red").save(path)
        image = load_image(path)
        result = AnalysisResult(DummyDepthBackend().predict(image), DummyNormalBackend().predict(image),
                                np.ones((image.height,image.width),np.uint32), DummyMaterialBackend().predict(image))
        with self.assertRaisesRegex(ValueError, "at least 2 x 2"):
            GeometryBuilder(grid_size=2).build(image, result)
