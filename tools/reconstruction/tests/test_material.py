"""Material contracts, color/normal semantics, atlas export and cross-language integration."""
from dataclasses import replace
import json
from pathlib import Path
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
import numpy as np
from PIL import Image
from pipeline.material_estimation_backend import NeutralMaterialBackend, srgb_to_linear, linear_to_srgb, validate_material
from pipeline.adapters.marigold import decode_prediction, MarigoldMaterialBackend
from pipeline.image_io import load_image
from pipeline.runner import ReconstructionPipeline
from pipeline.saved_prediction import remesh_saved, load_saved_geometry
from pipeline.saved_segmentation import load_saved_segmentation
from pipeline.material_assets import load_saved_material
from pipeline.material_rebuild import rebuild_geometry
from pipeline.geometry_builder import GeometryBuilder
from pipeline.scene_exporter import SceneExporter
from pipeline.types import AnalysisResult
from scene_package import load_package
from . import test_pipeline
from .support import make_input

PROPERTIES = {"target_names": ["albedo", "material"], "albedo": {"prediction_space": "srgb"},
    "material": {"sub_target_names": ["roughness", "metallicity", None]},
    "roughness": {"prediction_space": "linear"}, "metallicity": {"prediction_space": "linear"}}


class MaterialTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(prefix="material-tests-")
        self.root = Path(self.temp.name)
        self.image = load_image(make_input(self.root / "source.png", (32, 24)))

    def tearDown(self):
        self.temp.cleanup()

    def estimated(self):
        output = np.full((2, 24, 32, 3), .5, np.float32)
        output[1, :, :, 0] = .25
        output[1, :, :, 1] = .75
        output[1, :, :, 2] = 1  # Unused network channel must not become metallic or affect confidence.
        spread = np.full_like(output, .1)
        spread[1, :, :, 2] = 1
        return decode_prediction(self.image, output, spread, PROPERTIES, {"backend": "fixture"})

    def export(self):
        material = self.estimated()
        class Fixture:
            name = "material-fixture"
            def predict(self, image):
                return material
        return ReconstructionPipeline(material_backend=Fixture()).run(self.image.path, self.root / "scene", progress=lambda _: None)

    def test_neutral_fallback_never_copies_lit_photo(self):
        result = NeutralMaterialBackend().predict(self.image)
        validate_material(self.image, result)
        self.assertTrue((result.albedo == .5).all())
        self.assertFalse(np.array_equal(result.base_color, self.image.rgb))
        self.assertTrue((result.metallic == 0).all() and (result.confidence == 0).all())
        np.testing.assert_array_equal(result.normal[0, 0], [0, 0, 1])
        self.assertEqual(result.metadata["albedo_source"], "neutral-fallback")

    def test_model_channel_decode_and_linear_albedo(self):
        material = self.estimated()
        np.testing.assert_allclose(material.albedo, .21404114, atol=1e-7)
        np.testing.assert_allclose(material.roughness, .25)
        np.testing.assert_allclose(material.metallic, .75)
        np.testing.assert_allclose(material.confidence, .9)
        np.testing.assert_array_equal(material.base_color, 128)
        self.assertEqual(material.metadata["normal_confidence"], 0)

    def test_color_roundtrip_not_applied_to_data_maps(self):
        values = np.linspace(0, 1, 256, dtype=np.float32)
        np.testing.assert_allclose(linear_to_srgb(srgb_to_linear(values)), values, atol=1e-6)
        self.assertAlmostEqual(float(srgb_to_linear(np.float32(.5))), .21404114, places=6)
        self.assertEqual(float(self.estimated().roughness[0, 0]), .25)

    def test_missing_uncertainty_is_explicit_zero(self):
        values = np.full((2, 24, 32, 3), .5, np.float32)
        material = decode_prediction(self.image, values, None, PROPERTIES, {})
        self.assertFalse(material.confidence.any())
        self.assertIn("unavailable", material.metadata["confidence_semantics"])

    def test_bad_contract_color_spaces_and_normals(self):
        material = self.estimated()
        for changes in ({"albedo": material.albedo.astype(np.float64)}, {"roughness": material.roughness[:-1]},
                        {"metallic": np.full((24, 32), 1.1, np.float32)},
                        {"confidence": np.full((24, 32), np.nan, np.float32)},
                        {"normal": np.zeros((24, 32, 3), np.float32)},
                        {"metadata": {**material.metadata, "normal_space": "camera"}}):
            with self.assertRaises(ValueError):
                validate_material(self.image, replace(material, **changes))
        with self.assertRaisesRegex(ValueError, "parameterization"):
            decode_prediction(self.image, np.zeros((2, 24, 32, 3), np.float32), None,
                              {**PROPERTIES, "albedo": {"prediction_space": "linear"}}, {})

    def test_export_separates_original_and_binds_pbr_channels(self):
        root = self.export()
        obj = load_package(root)["objects"][0]
        m = obj["material"]
        self.assertEqual(m["albedoSource"], "intrinsic")
        self.assertEqual(m["normalStrength"], 0)
        for name, expected in (("originalImage", self.image.rgb), ("baseColor", 128), ("roughness", 64), ("metallic", 191)):
            with Image.open(root / m[name]) as image:
                np.testing.assert_array_equal(np.array(image), expected)
        with Image.open(root / "textures/object_metallic_roughness.png") as packed:
            np.testing.assert_array_equal(np.array(packed)[0, 0], [255, 64, 191])
        with Image.open(root / m["normal"]) as normal:
            np.testing.assert_array_equal(np.array(normal)[0, 0], [128, 128, 255])
        for validator in test_pipeline.VALIDATORS:
            result = subprocess.run([str(validator), str(root), "--objects"], capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            evidence = json.loads(result.stdout)["materialEvidence"][0]
            self.assertEqual(evidence["albedoSource"], "intrinsic")
            self.assertTrue(evidence["hasOriginalImage"])
            self.assertEqual(evidence["normalStrength"], 0)
            self.assertEqual(evidence["roughnessFactor"], 1)
            self.assertEqual(evidence["metallicFactor"], 1)

    def test_full_glb_embeds_intrinsic_albedo_orm_and_normal(self):
        root = self.export()
        data = (root / "meshes/scene_mesh.glb").read_bytes()
        length = struct.unpack_from("<I", data, 12)[0]
        gltf = json.loads(data[20:20+length])
        material = gltf["materials"][0]
        self.assertEqual(len(gltf["images"]), 3)
        self.assertIn("metallicRoughnessTexture", material["pbrMetallicRoughness"])
        self.assertEqual(material["normalTexture"]["scale"], 0)
        binary = data[20+length+8:]
        for item, path in zip(gltf["images"], ("object_albedo.png", "object_metallic_roughness.png", "object_normal.png")):
            view = gltf["bufferViews"][item["bufferView"]]
            self.assertEqual(binary[view["byteOffset"]:view["byteOffset"]+view["byteLength"]], (root / "textures" / path).read_bytes())

    def test_remesh_keeps_material_source_masks_and_float_maps(self):
        root = self.export()
        output = remesh_saved(root, self.root / "remeshed", GeometryBuilder())
        source_image, geometry, report = load_saved_geometry(output)
        np.testing.assert_array_equal(source_image.rgb, self.image.rgb)
        a, b = load_saved_material(root, self.image), load_saved_material(output, self.image)
        for key in ("albedo", "roughness", "metallic", "normal", "confidence"):
            np.testing.assert_array_equal(getattr(a, key), getattr(b, key))
        region = load_saved_segmentation(output, source_image, geometry)
        analysis = AnalysisResult(geometry.depth, geometry.normal, region.labels, b, geometry, region)
        mesh = rebuild_geometry(source_image, analysis, report)
        self.assertEqual(len(mesh.indices), report["geometry"]["triangles"])
        self.assertEqual(load_package(root)["objects"][0]["id"], load_package(output)["objects"][0]["id"])

    def test_material_paths_and_provenance_rejected_by_both_readers(self):
        root = self.export()
        data = load_package(root)
        for key, value in (("originalImage", "textures/../secret.png"), ("confidence", "textures/missing.png"),
                           ("normalSource", "camera"), ("albedoSource", "photo-is-albedo")):
            invalid = json.loads(json.dumps(data))
            invalid["objects"][0]["material"][key] = value
            (root / "scene.json").write_text(json.dumps(invalid), encoding="utf-8")
            with self.assertRaises((ValueError, OSError)):
                load_package(root)
            for validator in test_pipeline.VALIDATORS:
                result = subprocess.run([str(validator), str(root)], capture_output=True, timeout=30)
                self.assertNotEqual(result.returncode, 0)

    def test_optional_model_imports_and_cli_parameters(self):
        script = "import sys; from pipeline.adapters.marigold import MarigoldMaterialBackend; from pipeline.material_assets import load_saved_material; assert 'torch' not in sys.modules and 'diffusers' not in sys.modules"
        result = subprocess.run([sys.executable, "-c", script], cwd=Path(__file__).resolve().parents[1], capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        for kw in ({"resolution": 64}, {"steps": 0}, {"ensemble": 9}, {"seed": -1}):
            with self.assertRaises(ValueError):
                MarigoldMaterialBackend(**kw)

    def test_material_only_export_retains_camera_lights_look_and_object_pose(self):
        root = self.export()
        manifest = load_package(root)
        manifest["camera"]["position"][0] = .2
        manifest["look"]["exposure"] = 1.5
        manifest["lights"] = []
        manifest["objects"][0]["transform"]["position"][0] = 2
        manifest["objects"][0]["visible"] = False
        hdr = Path(__file__).resolve().parents[3]/"assets/environments/SoftStudio.hdr"
        (root/"textures/studio.hdr").write_bytes(hdr.read_bytes())
        manifest["environment"]["hdri"] = "textures/studio.hdr"
        (root/"scene.json").write_text(json.dumps(manifest),encoding="utf-8")
        image, geometry, report = load_saved_geometry(root)
        segmentation = load_saved_segmentation(root,image,geometry)
        material = NeutralMaterialBackend().predict(image)
        analysis = AnalysisResult(geometry.depth,geometry.normal,segmentation.labels,material,geometry,segmentation)
        mesh = rebuild_geometry(image,analysis,report)
        output = SceneExporter().export(self.root/"updated",image,analysis,mesh,report["backends"],source_package=root)
        result = load_package(output)
        for key in ("camera","environment","lights","look"):
            self.assertEqual(result[key],manifest[key])
        self.assertEqual(result["objects"][0]["transform"],manifest["objects"][0]["transform"])
        self.assertFalse(result["objects"][0]["visible"])
        self.assertEqual((output/"textures/studio.hdr").read_bytes(),hdr.read_bytes())
