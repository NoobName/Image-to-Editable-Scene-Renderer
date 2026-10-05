import copy
import json
import struct
import subprocess
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
import numpy as np
from runtime_dds import read_dds, write_dds
from analysis_contract import load_analysis
from appearance_contract import load_extension
from analysis_examples import make_examples
from export_analysis import export_saved
from . import test_pipeline


class AnalysisTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = TemporaryDirectory(prefix="analysis-maps-")
        cls.root = Path(cls.temp.name) / "fixtures"
        make_examples(cls.root)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def validate_cpp(self, root, expected):
        for validator in test_pipeline.VALIDATORS:
            result = subprocess.run([str(validator), str(root)], capture_output=True)
            self.assertEqual(result.returncode == 0, expected, result.stderr.decode("utf-8", errors="replace"))

    def test_numeric_fixtures_both_readers(self):
        for kind in ("plane", "tilted", "step"):
            root = self.root/kind
            data = load_analysis(root, load_extension(root))
            self.assertEqual(data["analysisSize"], [65, 33])
            self.assertEqual(data["sourceSize"], [195, 99])
            with np.load(root/"debug/geometry.npz") as saved:
                for key, name in (("depth", "depth"), ("normal", "normal"), ("position", "point_map")):
                    _, pixels = read_dds(root/data["maps"][key]["path"])
                    if pixels.ndim == 3:
                        pixels = pixels[..., :3]
                    np.testing.assert_array_equal(pixels, saved[name])
            self.validate_cpp(root, True)

    def test_dds_rejects_bad_headers_sizes_formats_and_nonfinite(self):
        root = self.root / "plane"
        path = root / "analysis/depth.dds"
        original = path.read_bytes()
        mutations = []
        for offset, value in ((0, 0), (4, 123), (16, 0), (20, 1), (28, 2), (128, 28), (132, 4), (136, 4), (140, 2), (144, 1), (148, 0x7fc00000), (148, 0x7f800000)):
            data = bytearray(original); struct.pack_into("<I", data, offset, value); mutations.append(data)
        mutations += [original[:-1], original+b"x"]
        try:
            for data in mutations:
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    load_analysis(root, load_extension(root))
                self.validate_cpp(root, False)
        finally:
            path.write_bytes(original)

    def test_semantics_mapping_space_identity_and_unit_normal_fail_closed(self):
        root = self.root / "plane"
        path = root / "analysis/analysis.json"
        original = path.read_text(encoding="utf-8")
        base = json.loads(original)
        mutations = []
        def change(keys, value):
            data = copy.deepcopy(base); target = data
            for key in keys[:-1]:
                target = target[key]
            target[keys[-1]] = value; mutations.append(data)
        change(["version"], 999)
        change(["sourceId"], "wrong")
        change(["sourceToAnalysis", 0], 1)
        change(["camera", "intrinsicsPixels", 0], 100)
        change(["camera", "cameraToWorld", 12], 1)
        change(["maps", "normal", "space"], "tangent")
        change(["maps", "position", "units"], "meters")
        change(["maps", "region", "sampling"], "guarded-bilinear")
        change(["maps", "depth", "path"], "analysis/../debug/depth.exr")
        change(["maps", "depth", "range"], [0, 100])
        try:
            for data in mutations:
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.assertRaises(ValueError):
                    load_analysis(root, load_extension(root))
                self.validate_cpp(root, False)
        finally:
            path.write_text(original, encoding="utf-8")
        normal = root/"analysis/normal.dds"; contents = normal.read_bytes()
        try:
            bad = bytearray(contents); struct.pack_into("<f", bad, 148+8, -.5); normal.write_bytes(bad)
            with self.assertRaises(ValueError):
                load_analysis(root, load_extension(root))
            self.validate_cpp(root, False)
        finally:
            normal.write_bytes(contents)

    def test_uint_ids_are_not_float_or_rgb_previews(self):
        path = self.root/"ids.dds"
        ids = np.array([[0, 1, 16777217, 4294967295]], dtype=np.uint32)
        write_dds(path, ids, "R32_UINT")
        fmt, actual = read_dds(path)
        self.assertEqual(fmt, "R32_UINT")
        np.testing.assert_array_equal(ids, actual)

    def test_saved_upgrade_preserves_scene_anchor_and_numeric_files(self):
        source = self.root/"step"
        output = self.root/"moved 升级"
        export_saved(source, output)
        for relative in ("scene.json", "textures/source_anchor.png", "relighting/relighting.json", "debug/depth.exr", "debug/geometry.npz", "debug/material.npz"):
            self.assertEqual((source/relative).read_bytes(), (output/relative).read_bytes())
        self.validate_cpp(output, True)
