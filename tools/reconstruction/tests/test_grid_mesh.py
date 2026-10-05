from dataclasses import replace
import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
import numpy as np
from pipeline.geometry_backend import GeometryPrediction, pinhole_points
from pipeline.geometry_builder import GeometryBuilder
from pipeline.types import InputImage, AnalysisResult, MaterialPrediction
from pipeline.runner import ReconstructionPipeline
from .support import make_input


class GridMeshTests(unittest.TestCase):
    def fixture(self, width=32, height=24, depth=None, mask=None):
        rgb = np.full((height,width,3),128,np.uint8)
        image = InputImage(Path("fixture.png"),rgb,(width,height),"synthetic")
        k = np.array([[height/width,0,.5],[0,1,.5],[0,0,1]],np.float32)
        z = np.full((height,width),2,np.float32) if depth is None else depth.copy()
        valid = np.ones_like(z,dtype=bool) if mask is None else mask.copy()
        z[~valid] = 0
        normals = np.zeros_like(rgb,dtype=np.float32)
        normals[valid,2] = -1
        prediction = GeometryPrediction(z,normals,pinhole_points(z,k),k,valid.astype(np.float32),valid,"synthetic")
        return image, prediction

    def mesh(self, image, prediction, **options):
        analysis = AnalysisResult(prediction.depth,prediction.normal,np.ones_like(prediction.depth,dtype=np.uint32),
                                  MaterialPrediction(image.rgb),prediction)
        mesh = GeometryBuilder(**options).build(image,analysis)
        stats = mesh.diagnostics
        self.assertEqual(stats["candidate_triangles"],len(mesh.indices)+sum(stats["rejected"].values()))
        self.assertLess(mesh.indices.max(),len(mesh.positions))
        return mesh

    def test_dense_pixel_centers_roundtrip_and_planar_winding(self):
        image,prediction = self.fixture()
        mesh = self.mesh(image,prediction)
        self.assertEqual(len(mesh.positions),32*24)
        self.assertEqual(len(mesh.indices),2*31*23)
        np.testing.assert_array_equal(mesh.positions,prediction.point_map.reshape(-1,3))
        p = mesh.positions
        projected = np.stack((prediction.camera_intrinsics[0,0]*p[:,0]/p[:,2]+.5,
                              .5-prediction.camera_intrinsics[1,1]*p[:,1]/p[:,2]),1)
        np.testing.assert_allclose(projected,mesh.texcoords,atol=1e-7)
        a,b,c = (p[mesh.indices[:,i]] for i in range(3))
        self.assertTrue((np.cross(b-a,c-a)[:,2]<0).all())
        self.assertEqual(mesh.diagnostics["isolated_vertices"],0)

    def test_three_valid_corners_choose_surviving_triangle(self):
        for missing in range(4):
            mask = np.ones((2,2),bool)
            mask.ravel()[missing] = False
            image,prediction = self.fixture(2,2,mask=mask)
            mesh = self.mesh(image,prediction)
            self.assertEqual(len(mesh.positions),3)
            self.assertEqual(len(mesh.indices),1)
            np.testing.assert_array_equal(mesh.positions,prediction.point_map[mask])

    def test_dense_mesh_across_multiple_row_batches(self):
        image,prediction = self.fixture(7,131)
        mesh = self.mesh(image,prediction)
        self.assertEqual(len(mesh.positions),7*131)
        self.assertEqual(len(mesh.indices),2*6*130)
        self.assertEqual(len(np.unique(mesh.indices)),len(mesh.positions))
        for row in range(130):
            triangle_rows = mesh.indices//7
            self.assertEqual(int(((triangle_rows.min(1)==row)&(triangle_rows.max(1)==row+1)).sum()),12)

    def test_isolated_valid_pixel_retained_but_never_connected(self):
        mask = np.zeros((5,5),bool)
        mask[:2,:2] = True
        mask[-1,-1] = True
        image,prediction = self.fixture(5,5,mask=mask)
        mesh = self.mesh(image,prediction)
        self.assertEqual(len(mesh.positions),5)
        self.assertEqual(len(mesh.indices),2)
        self.assertEqual(mesh.diagnostics["isolated_vertices"],1)
        self.assertNotIn(4,mesh.indices)

    def test_depth_step_never_connects_near_and_far(self):
        z = np.full((24,32),2,np.float32)
        z[:,16:] = 4
        image,prediction = self.fixture(depth=z)
        mesh = self.mesh(image,prediction)
        depths = mesh.positions[mesh.indices,2]
        self.assertTrue((depths.max(1)==depths.min(1)).all())
        self.assertEqual(mesh.diagnostics["rejected"]["depth"],2*23)
        # Camera translation produces larger image displacement on the near surface.
        near,far = mesh.positions[0],mesh.positions[-1]
        fx = prediction.camera_intrinsics[0,0]
        def displacement(p):
            return fx*(p[0]-.1)/p[2]-fx*p[0]/p[2]
        self.assertAlmostEqual(displacement(near)/displacement(far),2,places=5)

    def test_xy_outlier_filtered_even_with_constant_depth(self):
        image,prediction = self.fixture()
        points = prediction.point_map.copy()
        points[12,16,0] += 20
        mesh = self.mesh(image,replace(prediction,point_map=points))
        self.assertNotIn(12*32+16,mesh.indices)
        self.assertGreater(mesh.diagnostics["rejected"]["large_edge"],0)

    def test_absolute_limits_and_confidence(self):
        z = np.full((24,32),2,np.float32)
        z[:,16:] = 2.1
        image,prediction = self.fixture(depth=z)
        unfiltered = self.mesh(image,prediction)
        filtered = self.mesh(image,prediction,depth_edge_meters=.02)
        self.assertLess(len(filtered.indices),len(unfiltered.indices))
        # Flat patches have edge length and area proportional to Z and Z squared.
        z[:,16:] = 4
        image,prediction = self.fixture(depth=z)
        for options,reason in (({"max_edge_meters":.2},"large_edge"),({"max_triangle_area":.006},"large_area")):
            mesh = self.mesh(image,prediction,**options)
            self.assertGreater(mesh.diagnostics["rejected"][reason],0)
            self.assertTrue((mesh.positions[mesh.indices,2]==2).all())
        confidence = prediction.confidence.copy()
        confidence[0,0] = .1
        mesh = self.mesh(image,replace(prediction,confidence=confidence))
        self.assertEqual(len(mesh.positions),32*24-1)

    def test_coarse_grid_cannot_skip_internal_depth_spike(self):
        z = np.full((24,32),2,np.float32)
        z[:,15] = 6
        image,prediction = self.fixture(depth=z)
        with self.assertRaisesRegex(ValueError,"No valid triangles"):
            self.mesh(image,prediction,grid_size=2)

    def test_bad_limits_and_degenerate_faces(self):
        for options in ({"max_edge_stretch":0},{"max_edge_meters":-1},{"max_triangle_area":float("nan")},
                        {"depth_edge_meters":float("inf")},{"confidence_threshold":float("nan")}):
            with self.assertRaises(ValueError):
                GeometryBuilder(**options)
        image,prediction = self.fixture()
        points = prediction.point_map.copy()
        points[:,:,:2] = 0
        with self.assertRaisesRegex(ValueError,"No valid triangles"):
            self.mesh(image,replace(prediction,point_map=points))

    def test_offline_depth_remesh_without_torch_and_preserves_source(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            package = ReconstructionPipeline().run(make_input(root/"input.png",(32,24)),root/"source",progress=lambda _:None)
            original = (package/"meshes/scene_mesh.glb").read_bytes()
            script = Path(__file__).resolve().parents[1]/"remesh.py"
            process = subprocess.run([sys.executable,str(script),str(package),"--output",str(root/"output"),
                                      "--geometry-source","depth"],capture_output=True,timeout=30)
            self.assertEqual(process.returncode,0,process.stderr)
            npz_path = root/"output/debug/geometry.npz"
            with np.load(npz_path,allow_pickle=False) as data:
                np.testing.assert_array_equal(data["point_map"],pinhole_points(data["depth"],data["camera_intrinsics"]))
            self.assertEqual((package/"meshes/scene_mesh.glb").read_bytes(),original)
            report = json.loads((root/"output/debug/reconstruction.json").read_text())
            self.assertEqual(report["geometry"]["mesh_filtering"]["mode"],"pixel")
            self.assertEqual(report["geometry"]["vertices"],32*24)
            process = subprocess.run([sys.executable,"-c","import sys; import pipeline.saved_prediction; assert 'torch' not in sys.modules"],
                                      cwd=script.parent,capture_output=True)
            self.assertEqual(process.returncode,0,process.stderr)
