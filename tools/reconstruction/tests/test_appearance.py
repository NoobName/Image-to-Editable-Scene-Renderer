import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch
import numpy as np
from PIL import Image, ImageCms
from appearance_contract import load_extension, sha256_file, SIDECAR
from scene_package import load_package, write_package
from pipeline.image_io import load_image
from pipeline.runner import ReconstructionPipeline
from pipeline.geometry_builder import GeometryBuilder
from pipeline.saved_prediction import remesh_saved, load_saved_geometry
from . import test_pipeline
from .support import make_input


class AppearanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(prefix="appearance-")
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def package(self, size=(96,64), max_size=64):
        source = make_input(self.root/"中文细字.png",size)
        return ReconstructionPipeline(geometry_builder=GeometryBuilder(grid_size=9)).run(source,self.root/"原包",max_size=max_size,progress=lambda _:None)

    def cpp(self, root, accepted, expected=None):
        for validator in test_pipeline.VALIDATORS:
            run = subprocess.run([str(validator),str(root),"--appearance"],capture_output=True,timeout=30)
            self.assertEqual(run.returncode==0,accepted,run.stderr.decode("utf-8",errors="replace"))
            if accepted and expected is not None:
                self.assertEqual(json.loads(run.stdout),expected)
            if not accepted:
                self.assertIn("relighting/relighting.json",run.stderr.decode("utf-8",errors="replace"))

    def test_1500_anchor_and_512_analysis_and_resolution_independent_identity(self):
        package = self.package((1500,1000),512)
        a = load_extension(package)
        self.assertEqual(a["sourceImage"]["size"],[1500,1000])
        self.assertEqual(a["analysisImage"]["size"],[512,341])
        with np.load(package/"debug/geometry.npz") as arrays:
            self.assertEqual(arrays["depth"].shape,(341,512))
        second = ReconstructionPipeline(geometry_builder=GeometryBuilder(grid_size=9)).run(self.root/"中文细字.png",self.root/"小分析",max_size=256,progress=lambda _:None)
        b = load_extension(second)
        self.assertEqual(a["sourceId"],b["sourceId"])
        self.assertEqual(a["sourceImage"],b["sourceImage"])
        self.assertNotEqual(a["analysisImage"]["sha256"],b["analysisImage"]["sha256"])
        self.assertEqual((package/a["originalFile"]["path"]).read_bytes(),(self.root/"中文细字.png").read_bytes())
        self.cpp(package,True,a)

    def test_all_exif_pixel_centers(self):
        pixels=np.arange(3*2*3,dtype=np.uint8).reshape(2,3,3)
        for orientation in range(1,9):
            with self.subTest(orientation=orientation):
                source=Image.fromarray(pixels);exif=Image.Exif();exif[274]=orientation
                path=self.root/f"旋转{orientation}.png";source.save(path,exif=exif)
                image=load_image(path)
                transform=np.array(image.normalization["storedToCanonical"]).reshape(3,3)
                for y in range(2):
                    for x in range(3):
                        nx,ny,_=transform@[x+.5,y+.5,1]
                        np.testing.assert_array_equal(image.canonical_rgb[int(ny-.5),int(nx-.5)],pixels[y,x])
                self.assertEqual(image.canonical_rgb.shape[:2],(3,2) if orientation>=5 else (2,3))

    def test_icc_alpha_and_invalid_icc(self):
        source=Image.new("RGBA",(48,32),(10,20,30,0));source.putpixel((4,3),(60,80,100,255))
        path=self.root/"颜色.png";source.save(path,icc_profile=ImageCms.ImageCmsProfile(ImageCms.createProfile("sRGB")).tobytes())
        image=load_image(path,16)
        self.assertEqual(image.canonical_rgb.shape,(32,48,3))
        self.assertEqual(image.normalization["iccPolicy"],"embedded-to-srgb")
        np.testing.assert_array_equal(image.canonical_rgb[0,0],[255,255,255])
        np.testing.assert_array_equal(image.canonical_rgb[3,4],[60,80,100])
        output=ReconstructionPipeline(geometry_builder=GeometryBuilder(grid_size=9)).run(path,self.root/"profile",max_size=32,progress=lambda _:None)
        self.cpp(output,True,load_extension(output))
        source.save(path,icc_profile=b"invalid profile")
        with self.assertRaises(ValueError):load_image(path)

    def test_source_limits_and_high_bit_depth_are_explicit(self):
        path=make_input(self.root/"input.png",(32,32))
        with patch("pipeline.image_io.MAX_WORKING_BYTES",1),self.assertRaisesRegex(ValueError,"working-memory"):
            load_image(path)
        with patch("pipeline.image_io.MAX_DIMENSION",31),self.assertRaisesRegex(ValueError,"will not be downscaled"):
            load_image(path)
        path=self.root/"16bit.png";Image.fromarray(np.array([[0,65535],[1024,32768]],np.uint16)).save(path)
        image=load_image(path)
        self.assertEqual(image.normalization["precision"],"rgb8-normalized-not-high-bit-depth")
        self.assertEqual(image.original_bytes,path.read_bytes())

    def test_move_repack_and_edited_scene_camera_do_not_rewrite_source_camera(self):
        package=self.package()
        original=load_extension(package)
        before=(package/SIDECAR).read_bytes()
        data=load_package(package);data["camera"]["position"]=[2,1,-3];write_package(package,data)
        moved=self.root/"搬移 场景";package.rename(moved)
        self.assertEqual(load_extension(moved),original)
        remeshed=remesh_saved(moved,self.root/"remesh",GeometryBuilder(grid_size=9))
        self.assertEqual((remeshed/SIDECAR).read_bytes(),before)
        scripts=Path(__file__).resolve().parents[1]
        for script,options in (("segment.py",["--segmentation-backend","dummy","--grid-size","9"]),
                               ("estimate_material.py",["--material-backend","neutral"])):
            target=self.root/script.removesuffix(".py")
            run=subprocess.run([sys.executable,str(scripts/script),str(moved),"--output",str(target),*options],capture_output=True,timeout=60)
            self.assertEqual(run.returncode,0,run.stderr.decode("utf-8",errors="replace"))
            self.assertEqual((target/SIDECAR).read_bytes(),before)
            for key in ("sourceImage","analysisImage","originalFile"):
                relative=original[key]["path"]
                self.assertEqual((target/relative).read_bytes(),(moved/relative).read_bytes())
            self.cpp(target,True,original)
        estimated=load_package(self.root/"estimate_material")
        self.assertEqual(estimated["camera"]["position"],[2,1,-3])
        self.assertEqual(original["sourceCamera"]["cameraToWorld"],[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1])

    def test_legacy_upgrade_is_processed_only_and_requires_calibration(self):
        package=self.package()
        # Only mutate this owned temporary fixture, never a user's old package.
        (package/SIDECAR).unlink()
        # A pre-extension fixture cannot retain the later analysis contract, which requires its anchor.
        shutil.rmtree(package/"analysis", ignore_errors=True)
        # A synthetic pre-Prompt16 fixture also predates the lighting extension.
        self.assertTrue((package/"lighting").resolve().is_relative_to(self.root.resolve()))
        shutil.rmtree(package/"lighting", ignore_errors=True)
        self.assertIsNone(load_extension(package));load_package(package)
        output=remesh_saved(package,self.root/"upgraded",GeometryBuilder(grid_size=9))
        data=load_extension(output)
        self.assertEqual(data["sourceKind"],"legacy-processed")
        self.assertEqual(data["sourceImage"]["size"],data["analysisImage"]["size"])
        self.assertEqual(data["sourceCamera"]["status"],"calibration-required")
        self.assertFalse(data["capabilities"]["fullResolutionAnchor"])
        self.cpp(output,True,data)
        with self.assertRaises(FileExistsError):remesh_saved(package,package,GeometryBuilder(grid_size=9))

    def test_strict_contract_rejected_by_both_readers(self):
        package=self.package();base=load_extension(package)
        mutations={"version":lambda d:d.update(version=2),"unknown":lambda d:d.update(lighting={}),
            "size":lambda d:d["sourceImage"]["size"].__setitem__(0,97),
            "hash":lambda d:d["sourceImage"].update(sha256="0"*64),
            "bad-hash":lambda d:d["sourceImage"].update(sha256="g"*64),
            "color":lambda d:d["analysisImage"].update(colorSpace="linear"),
            "path":lambda d:d["sourceImage"].update(path="textures/../escape.png"),
            "absolute":lambda d:d["sourceImage"].update(path="D:/image.png"),
            "mapping":lambda d:d["analysisMapping"]["sourceToAnalysis"].__setitem__(2,.5),
            "nan":lambda d:d["analysisMapping"]["sourceToAnalysis"].__setitem__(0,float("nan")),
            "inf":lambda d:d["sourceCamera"]["intrinsicsNormalized"].__setitem__(0,float("inf")),
            "intrinsics":lambda d:d["sourceCamera"]["sourceIntrinsicsPixels"].__setitem__(0,7),
            "pose":lambda d:d["sourceCamera"]["cameraToWorld"].__setitem__(12,2),
            "capability":lambda d:d["capabilities"].update(requiresCalibration=False),
            "source-id":lambda d:d.update(sourceId="wrong"),
            "orientation":lambda d:d["normalization"].update(exifOrientation=6),
            "legacy-claim":lambda d:d.update(sourceKind="legacy-processed"),
            "bool-size":lambda d:d["sourceImage"]["size"].__setitem__(0,True)}
        for name,mutate in mutations.items():
            with self.subTest(name=name):
                data=copy.deepcopy(base);mutate(data)
                (package/SIDECAR).write_text(json.dumps(data),encoding="utf-8")
                with self.assertRaises(ValueError):load_package(package)
                self.cpp(package,False)
        (package/SIDECAR).write_text(json.dumps(base),encoding="utf-8")
        self.cpp(package,True,base)

    def test_damage_is_not_absence_and_never_published(self):
        package=self.package();path=package/SIDECAR;path.write_text('{"version":1,"version":1}',encoding="utf-8")
        with self.assertRaises(ValueError):load_extension(package)
        self.cpp(package,False)
        with self.assertRaises(ValueError):remesh_saved(package,self.root/"bad-output",GeometryBuilder(grid_size=9))
        self.assertFalse((self.root/"bad-output/scene.json").exists())
        path.unlink();path.mkdir()
        with self.assertRaises(ValueError):load_extension(package)
        self.cpp(package,False)

    def test_truncated_payload_is_not_a_valid_image(self):
        package=self.package();data=load_extension(package)
        path=package/data["sourceImage"]["path"]
        path.write_bytes(path.read_bytes()[:50])
        data["sourceImage"]["sha256"]=sha256_file(path)
        (package/SIDECAR).write_text(json.dumps(data),encoding="utf-8")
        with self.assertRaises((ValueError,OSError)):load_package(package)
        self.cpp(package,False)

    def test_symlink_boundary(self):
        package=self.package();data=load_extension(package)
        path=package/data["sourceImage"]["path"];outside=self.root/"outside.png";shutil.copyfile(path,outside)
        path.unlink()
        try:path.symlink_to(outside)
        except OSError:
            self.skipTest("Windows symlink privilege unavailable; traversal rejection tested separately")
        with self.assertRaises(ValueError):load_package(package)
        self.cpp(package,False)
