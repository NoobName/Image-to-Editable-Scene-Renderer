"""Mask resolution, identity, decomposition and real C++ interchange tests; no model needed."""
from dataclasses import replace
import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
import numpy as np
from PIL import Image
from pipeline.geometry_backend import DummyGeometryBackend,pinhole_points
from pipeline.geometry_builder import GeometryBuilder
from pipeline.image_io import load_image
from pipeline.segmentation_types import MaskProposal,SegmentationPrediction
from pipeline.scene_decomposer import resolve_regions,split_mesh
from pipeline.segmentation_prompts import load_prompts
from pipeline.scene_exporter import SceneExporter
from pipeline.saved_prediction import remesh_saved
from pipeline.types import AnalysisResult,MaterialPrediction
from scene_package import load_package,load_region
from .support import make_input
from . import test_pipeline


class SegmentationTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(prefix="segmentation-tests-")
        self.root = Path(self.temp.name)
        self.image = load_image(make_input(self.root/"input.png",(32,24)))
        self.geometry = DummyGeometryBackend().predict(self.image)

    def tearDown(self):
        self.temp.cleanup()

    def fixture(self):
        mask = self.geometry.valid_mask.copy()
        mask[:4] = False
        depth,normal,points = [array.copy() for array in (self.geometry.depth,self.geometry.normal,self.geometry.point_map)]
        depth[~mask],normal[~mask],points[~mask] = 0,0,0
        geometry = replace(self.geometry,depth=depth,normal=normal,point_map=points,valid_mask=mask,confidence=mask.astype(np.float32))
        sky = ~mask
        left = mask.copy();left[:,16:] = False
        right = mask & ~left
        prediction = SegmentationPrediction((MaskProposal(sky,object_id="sky",name="Sky",category="sky"),
            MaskProposal(left,object_id="building",name="Building",category="major"),
            MaskProposal(right,object_id="ground",name="Ground",category="ground")),{"prompted":True})
        segmentation = resolve_regions(self.image,prediction,geometry,min_pixels=1)
        analysis = AnalysisResult(geometry.depth,geometry.normal,segmentation.labels,MaterialPrediction(self.image.rgb),geometry,segmentation)
        mesh = GeometryBuilder().build(self.image,analysis)
        return analysis,mesh

    def export(self):
        analysis,mesh = self.fixture()
        output = SceneExporter().export(self.root/"scene",self.image,analysis,mesh,{"geometry":"test","segmentation":"test"})
        return output,analysis,mesh

    def test_region_statistics_ignore_invalid_depth(self):
        analysis,_ = self.fixture()
        sky,building,ground = analysis.segmentation.regions
        self.assertIsNone(sky.average_depth)
        self.assertEqual(sky.valid_depth_pixels,0)
        self.assertEqual(sky.bounding_box,(0,0,32,4))
        self.assertEqual(building.bounding_box,(0,4,16,20))
        self.assertEqual(building.average_depth,3)
        self.assertEqual(ground.valid_depth_pixels,320)
        self.assertTrue((analysis.labels>0).all())
        np.testing.assert_array_equal(sum(r.mask.astype(int) for r in analysis.segmentation.regions),1)

    def test_prompt_overlap_priority_and_background_remainder(self):
        a = np.zeros((24,32),bool);a[3:15,4:20] = True
        b = np.zeros_like(a);b[7:18,12:28] = True
        result = resolve_regions(self.image,SegmentationPrediction((MaskProposal(a,object_id="a",name="Tree",category="major"),
            MaskProposal(b,object_id="b",name="Tree",category="major")),{"prompted":True}),self.geometry,min_pixels=1)
        self.assertEqual([r.name for r in result.regions],["Tree","Tree 2","Background"])
        np.testing.assert_array_equal(result.regions[0].mask,a)
        np.testing.assert_array_equal(result.regions[1].mask,b & ~a)
        self.assertEqual(len(np.unique(result.labels)),3)

    def test_automatic_overlap_and_ids_are_deterministic(self):
        small = np.zeros((24,32),bool);small[4:12,8:16] = True
        large = np.ones_like(small)
        def run(items):
            return resolve_regions(self.image,SegmentationPrediction(tuple(MaskProposal(m) for m in items)),self.geometry,min_pixels=1)
        first,second = run([large,small]),run([small,large])
        np.testing.assert_array_equal(first.labels,second.labels)
        self.assertEqual([r.object_id for r in first.regions],[r.object_id for r in second.regions])
        np.testing.assert_array_equal(first.regions[0].mask,small)

    def test_bad_masks_ids_limits_and_no_silent_named_truncation(self):
        mask = np.ones((24,32),bool)
        for proposal in (MaskProposal(mask.astype(np.uint8)),MaskProposal(mask[:-1]),MaskProposal(mask,score=float("nan")),
                         MaskProposal(mask,category="nonsense"),MaskProposal(mask,object_id="../unsafe")):
            with self.assertRaises(ValueError):
                resolve_regions(self.image,SegmentationPrediction((proposal,)),self.geometry)
        with self.assertRaises(ValueError):
            resolve_regions(self.image,SegmentationPrediction((MaskProposal(mask,object_id="same"),MaskProposal(mask,object_id="same"))),self.geometry)
        with self.assertRaisesRegex(ValueError,"max-regions"):
            resolve_regions(self.image,SegmentationPrediction((MaskProposal(mask,object_id="a"),MaskProposal(mask,object_id="b")),{"prompted":True}),self.geometry,max_regions=2)
        with self.assertRaises(ValueError):
            resolve_regions(self.image,SegmentationPrediction(()),self.geometry,min_pixels=0)

    def test_split_keeps_positions_uvs_and_never_crosses_regions(self):
        analysis,mesh = self.fixture()
        meshes,stats = split_mesh(mesh,analysis.segmentation)
        self.assertIsNone(meshes["sky"])
        self.assertEqual(stats["assigned_triangles"]+stats["boundary_triangles_removed"],len(mesh.indices))
        self.assertEqual(stats["boundary_triangles_removed"],2*19)
        for region in analysis.segmentation.regions[1:]:
            part = meshes[region.object_id]
            x = (part.texcoords[:,0]*32).astype(int);y = (part.texcoords[:,1]*24).astype(int)
            np.testing.assert_array_equal(part.positions,analysis.geometry.point_map[y,x])
            self.assertTrue(region.mask[y,x].all())
            self.assertTrue((analysis.labels[y[part.indices],x[part.indices]]==region.label_id).all())

    def test_coarse_grid_cannot_skip_an_interior_object(self):
        small = np.zeros((24,32),bool);small[10:14,14:18] = True
        seg = resolve_regions(self.image,SegmentationPrediction((MaskProposal(small,object_id="small",name="Small",category="major"),),{"prompted":True}),self.geometry)
        analysis = AnalysisResult(self.geometry.depth,self.geometry.normal,seg.labels,MaterialPrediction(self.image.rgb),self.geometry,seg)
        mesh = GeometryBuilder(grid_size=2).build(self.image,analysis)
        parts,stats = split_mesh(mesh,seg)
        self.assertTrue(all(part is None for part in parts.values()))
        self.assertEqual(stats["boundary_triangles_removed"],2)

    def test_sidecars_masks_and_cpp_logical_objects(self):
        root,analysis,_ = self.export()
        manifest = load_package(root)
        self.assertEqual([obj["id"] for obj in manifest["objects"]],["sky","building","ground"])
        for obj,region in zip(manifest["objects"],analysis.segmentation.regions):
            metadata = load_region(root,obj)
            with Image.open(root/metadata["mask"]) as stored:
                np.testing.assert_array_equal(np.array(stored)>0,region.mask)
            self.assertEqual(metadata["averageDepth"],region.average_depth)
        self.assertNotIn("mesh",manifest["objects"][0])
        for validator in test_pipeline.VALIDATORS:
            process = subprocess.run([str(validator),str(root),"--objects"],capture_output=True,timeout=30)
            self.assertEqual(process.returncode,0,process.stderr)
            report = json.loads(process.stdout)
            self.assertEqual(report["textureCount"],1,"Object materials should reuse one photo texture")
            sky,building,ground = report["objects"]
            self.assertEqual(sky["renderers"],[])
            self.assertIsNone(sky["averageDepth"])
            self.assertEqual(building["name"],"Building")
            self.assertTrue(set(building["materials"]).isdisjoint(ground["materials"]))
            self.assertTrue(building["renderers"] and ground["renderers"])

    def test_metadata_rejected_consistently_by_python_and_cpp(self):
        root,_,_ = self.export()
        manifest = load_package(root)
        sidecar = root/manifest["objects"][0]["region"]
        original = json.loads(sidecar.read_text())
        cases = [{**original,"id":"wrong"},{**original,"boundingBox":[0,0,0,4]},
                 {**original,"labelId":1.5},{**original,"mask":"objects/../masks/segmentation.png"},
                 {**original,"averageDepth":3},{**original,"triangleCount":1},
                 {**original,"pixelCount":1000000},{**original,"name":"Wrong"}]
        for data in cases:
            sidecar.write_text(json.dumps(data),encoding="utf-8")
            with self.assertRaises(ValueError):
                load_package(root)
            for validator in test_pipeline.VALIDATORS:
                process = subprocess.run([str(validator),str(root)],capture_output=True,timeout=30)
                self.assertNotEqual(process.returncode,0)

    def test_remesh_preserves_region_identity_and_masks_without_model(self):
        root,analysis,_ = self.export()
        output = remesh_saved(root,self.root/"remeshed",GeometryBuilder(),"depth")
        first,second = load_package(root),load_package(output)
        self.assertEqual([obj["id"] for obj in first["objects"]],[obj["id"] for obj in second["objects"]])
        for obj in second["objects"]:
            data = load_region(output,obj)
            self.assertEqual((output/data["mask"]).read_bytes(),(root/data["mask"]).read_bytes())
        script="import sys; from pipeline.adapters.sam2 import Sam2SegmentationBackend; from pipeline.saved_segmentation import load_saved_segmentation; assert 'torch' not in sys.modules"
        process=subprocess.run([sys.executable,"-c",script],cwd=Path(__file__).resolve().parents[1],capture_output=True)
        self.assertEqual(process.returncode,0,process.stderr)

    def test_named_prompt_validation(self):
        path=self.root/"prompts.json"
        valid={"version":1,"regions":[{"id":"building","name":"Building","category":"major","points":[[.4,.5,1]],"box":[.1,.2,.7,.8]}]}
        path.write_text(json.dumps(valid),encoding="utf-8")
        self.assertEqual(load_prompts(path)[0]["id"],"building")
        for changes in ({"points":[[1.1,.5,1]]},{"box":[.7,.2,.1,.8]},{"id":"../x"},{"category":"unknown"},{"points":[[.5,.5,True]]}):
            path.write_text(json.dumps({"version":1,"regions":[{**valid["regions"][0],**changes}]}),encoding="utf-8")
            with self.assertRaises(ValueError):load_prompts(path)
