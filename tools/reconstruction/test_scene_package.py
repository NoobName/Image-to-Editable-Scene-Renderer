"""Manifest, transactional I/O and Python -> C++ interoperability regression tests."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from scene_package import load_package, write_package, add_auxiliary, copy_asset
from package_schema import read_json, SCHEMA

REPO = Path(__file__).resolve().parents[2]
FIXTURE = REPO / "assets/ScenePackage"
VALIDATOR = None
if "--validator" in sys.argv:
    index = sys.argv.index("--validator")
    VALIDATOR = Path(sys.argv[index+1]).resolve()
    del sys.argv[index:index+2]


class ScenePackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="scene-package-", dir=REPO / "generated")
        self.root = Path(self.temp.name) / "移动后的包 with spaces"
        shutil.copytree(FIXTURE, self.root)
        self.data = read_json(self.root / "scene.json")

    def tearDown(self):
        self.temp.cleanup()

    def save_raw(self, data):
        (self.root / "scene.json").write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")

    def cpp(self):
        if VALIDATOR is None:
            self.skipTest("C++ validator not supplied; use --validator build/Debug/ValidateScenePackage.exe")
        return subprocess.run([str(VALIDATOR), str(self.root)], capture_output=True, encoding="utf-8", timeout=20)

    def test_roundtrip_and_relocation(self):
        expected = write_package(self.root, self.data)
        self.assertEqual(expected, load_package(self.root / "scene.json"))
        result = self.cpp()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(expected, json.loads(result.stdout))

    def test_minimal_and_defaults(self):
        for obj in self.data["objects"]:
            obj.pop("transform", None)
            obj.pop("visible", None)
            obj.pop("material", None)
        self.data.pop("look")
        self.data.pop("auxiliary")
        self.data["environment"] = {}
        self.data["lights"] = []
        self.data["camera"] = {"position": [0, 0, -5], "target": [0, 0, 0]}
        expected = write_package(self.root, self.data)
        result = self.cpp()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(expected, json.loads(result.stdout))

    def test_empty_scene(self):
        self.data["objects"] = []
        self.save_raw(self.data)
        self.assertEqual(load_package(self.root)["objects"], [])
        self.assertEqual(self.cpp().returncode, 0)

    def test_invalid_contract_both_languages(self):
        cases = {
            "version": lambda d: d.update(version=2),
            "bool-version": lambda d: d.update(version=True),
            "unknown": lambda d: d.update(typo=1),
            "required": lambda d: d.pop("camera"),
            "units": lambda d: d.update(units="centimeters"),
            "camera-range": lambda d: d["camera"].update(near=100, far=10),
            "degenerate-camera": lambda d: d["camera"].update(target=d["camera"]["position"]),
            "vertical-camera": lambda d: d["camera"].update(position=[0, 0, 0], target=[0, 1, 0]),
            "zero-light": lambda d: d["lights"][0].update(direction=[0, 0, 0]),
            "light-limit": lambda d: d.update(lights=d["lights"]*5),
            "negative-light": lambda d: d["lights"][1].update(intensity=-1),
            "light-fields": lambda d: d["lights"][0].update(position=[0, 0, 0]),
            "duplicate-name": lambda d: d["objects"][1].update(name=d["objects"][0]["name"]),
            "zero-scale": lambda d: d["objects"][0]["transform"].update(scale=[1, 0, 1]),
            "vector-size": lambda d: d["objects"][0]["transform"].update(position=[0, 1]),
            "look-range": lambda d: d["look"].update(exposure=17),
            "look-bool": lambda d: d["look"].update(exposure=True),
            "tone-enum": lambda d: d["look"].update({"tone-mapping": "oops"}),
            "bad-texture": lambda d: d["objects"][1]["material"].update(baseColor="textures/missing.png"),
            "color-range": lambda d: d["objects"][1]["material"].update(baseColorFactor=[2, 1, 1, 1]),
            "absolute-path": lambda d: d["objects"][0].update(mesh="C:/model.glb"),
            "traversal": lambda d: d["objects"][0].update(mesh="meshes/../meshes/MaterialLab.glb"),
            "wrong-folder": lambda d: d["objects"][0].update(mesh="textures/MaterialLab.glb"),
            "wrong-slash": lambda d: d["objects"][0].update(mesh="meshes\\MaterialLab.glb"),
            "url": lambda d: d["environment"].update(hdri="https://example.com/map.hdr"),
            "missing-aux": lambda d: d["auxiliary"].update(depth="debug/missing.exr"),
            "float-overflow": lambda d: d["camera"].update(far=1e300),
        }
        for name, mutate in cases.items():
            with self.subTest(name=name):
                value = copy.deepcopy(self.data)
                mutate(value)
                self.save_raw(value)
                with self.assertRaises((ValueError, OSError)):
                    load_package(self.root)
                result = self.cpp()
                self.assertNotEqual(result.returncode, 0, f"C++ accepted {name}")

    def test_strict_json(self):
        for text in ('{"version":1,"version":1}', '{"version":NaN}', '{"version":1e999}', '{"version":1} trailing'):
            with self.subTest(text=text):
                (self.root / "scene.json").write_text(text, encoding="utf-8")
                with self.assertRaises(ValueError):
                    load_package(self.root)
                self.assertNotEqual(self.cpp().returncode, 0)

    def test_failed_write_preserves_manifest(self):
        previous = (self.root / "scene.json").read_bytes()
        self.data["camera"]["far"] = -1
        with self.assertRaises(ValueError):
            write_package(self.root, self.data)
        self.assertEqual(previous, (self.root / "scene.json").read_bytes())
        self.assertFalse(list(self.root.glob("*.tmp")))

    def test_auxiliary_is_copied_verbatim(self):
        # This intentionally opaque payload tests transport, not EXR decoding.
        source = Path(self.temp.name) / "source.exr"
        source.write_bytes(b"opaque-analysis-payload\x00\xff")
        for kind in ("depth", "normal", "pointmap"):
            relative = add_auxiliary(self.root, self.data, kind, source)
            self.assertEqual((self.root / relative).read_bytes(), source.read_bytes())
        expected = write_package(self.root, self.data)
        self.assertEqual(expected, json.loads(self.cpp().stdout))

    def test_copy_does_not_overwrite(self):
        source = Path(self.temp.name) / "other.png"
        source.write_bytes(b"different")
        target = self.root / "textures/baseColor.png"
        before = target.read_bytes()
        with self.assertRaises(FileExistsError):
            copy_asset(self.root, source, "textures/baseColor.png")
        self.assertEqual(target.read_bytes(), before)

    def test_cpp_rejects_mismatched_scalar_map_dimensions(self):
        # Python validates the transport contract; image decoding is a separate C++ layer.
        self.data["objects"][1]["material"]["roughness"] = "textures/normal.png"
        self.save_raw(self.data)
        load_package(self.root)
        result = self.cpp()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("equal dimensions", result.stderr)

    def test_gltf_external_dependency_boundary(self):
        # Our original .gltf has external BIN/images; all must remain inside the package.
        model_dir = self.root / "meshes/external"
        shutil.copytree(REPO / "assets/models/MaterialLab", model_dir)
        self.data["objects"][0]["mesh"] = "meshes/external/MaterialLab.gltf"
        self.save_raw(self.data)
        self.assertEqual(self.cpp().returncode, 0)
        load_package(self.root)
        gltf = read_json(model_dir / "MaterialLab.gltf")
        outside = Path(self.temp.name) / "outside.bin"
        shutil.copyfile(model_dir / "MaterialLab.bin", outside)
        gltf["buffers"][0]["uri"] = "../../../outside.bin"
        (model_dir / "MaterialLab.gltf").write_text(json.dumps(gltf), encoding="utf-8")
        with self.assertRaises(ValueError):
            load_package(self.root)
        self.assertNotEqual(self.cpp().returncode, 0)

    def test_symlink_escape(self):
        source = Path(self.temp.name) / "outside.glb"
        shutil.copyfile(self.root / "meshes/MaterialLab.glb", source)
        link = self.root / "meshes/link.glb"
        try:
            link.symlink_to(source)
        except OSError as error:
            self.skipTest(f"Symlink privilege unavailable: {error}")
        self.data["objects"][0]["mesh"] = "meshes/link.glb"
        self.save_raw(self.data)
        with self.assertRaises(ValueError):
            load_package(self.root)
        self.assertNotEqual(self.cpp().returncode, 0)

    def test_schema_contains_portable_standard_keywords(self):
        self.assertEqual(SCHEMA["$schema"], "https://json-schema.org/draft/2020-12/schema")
        self.assertEqual(SCHEMA["properties"]["version"]["const"], 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
