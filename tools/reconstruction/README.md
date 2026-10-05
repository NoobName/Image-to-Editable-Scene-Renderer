# Reconstruction Pipeline

Stage 13 adds [intrinsic PBR material estimation](Material.md) through `MaterialEstimationBackend`. Use `--material-backend marigold` for real albedo/roughness/metallic estimation. New CLI/API calls default to a neutral material, with the original photograph stored separately for **Original Image** view. The older photo-placeholder behavior described below remains only for legacy packages/tests. The no-model path still needs no PyTorch.

Stage 12 adds replaceable Dummy / SAM 2 segmentation, named point/box prompts, region statistics and independently editable object meshes. See [Segmentation.md](Segmentation.md). The default CLI still selects Dummy segmentation; opt in with `--segmentation-backend sam2`. The sections below document the original Dummy path where indicated.

Stage 11 makes per-pixel meshing the default and exports a standalone textured `meshes/scene_mesh.glb`. Read [Geometry.md](Geometry.md) for discontinuity/edge/area filtering, saved-prediction remeshing, Wireframe/Depth/Normal views and parallax validation.

Stage 10 adds an optional unified geometry backend. See [MoGe integration](MoGe.md) for real inference, setup, output conventions and validation. This page describes the default Dummy path, which still requires no PyTorch or model weights. Both paths now export unified numeric arrays and `depth.png`, `normal.png`, `confidence.png` previews.

JPEG/PNG → four interchangeable Dummy backends → textured grid → ScenePackage v1 → existing DX12 Renderer.

The default Dummy path validates file interchange without model inference. It does not estimate depth, hidden geometry, segmentation, intrinsic materials or calibrated camera parameters, and requires no PyTorch/CUDA. Optional real geometry and segmentation run behind Python adapters; no path uses RPC or renderer bindings.

## Environment

For the dedicated Miniconda CUDA environment, use `conda activate image-scene-renderer` and run `python reconstruct.py ...`. Its creation spec is [environment.yml](environment.yml); setup and GPU instructions are in [MoGe.md](MoGe.md). The local venv below remains an independent alternative for the original workflow. Validation scripts accept `-Python python` after Conda activation.

Use a separate local virtual environment (Python 3.12+, tested with 3.13.2 x64):

```powershell
# From the repository root:
.\tools\reconstruction\setup.ps1
# To select an installed interpreter explicitly:
.\tools\reconstruction\setup.ps1 -Python "D:\miniconda\python.exe"
```

`setup.ps1` creates `.venv/` here and installs only the pinned wheels in `requirements.txt`: NumPy, Pillow, OpenEXR. It does not install into global Python or download model weights. `.venv/` is ignored by Git. The C++ build and Renderer remain independent of this environment; the original `scene_package.py` file API still uses the standard library alone.

## Run

From the repository root, without activating the environment:

```powershell
.\tools\reconstruction\.venv\Scripts\python.exe tools/reconstruction/reconstruct.py "input.jpg" --output "generated/scene01"
.\build\Debug\ImageSceneRenderer.exe --package "generated/scene01"
```

Or from this directory:

```powershell
.\.venv\Scripts\Activate.ps1
python reconstruct.py "input.jpg" --output "generated/scene01"
```

Input/output paths resolve against the **current working directory**. In the second example, output is `tools/reconstruction/generated/scene01`, so pass that actual path to the Renderer. If activation is restricted, use `.\.venv\Scripts\python.exe reconstruct.py ...`; no execution-policy change is necessary.

Output must be new or empty. Existing scenes are never overwritten. Use a different directory for each experiment. A complete package is prepared in a temporary sibling directory and published only after successful schema/file validation.

| Option | Default | Meaning |
| --- | --- | --- |
| `--output` | required | New/empty package directory |
| `--geometry-backend` | `dummy` | `dummy` or `moge`; legacy `--backend dummy` remains an alias |
| `--max-size` | 1024 | Longest processed image edge, 16–2048; never upscales |
| `--grid-size` | omitted | Every valid pixel; optional 2–257 selects a coarse mesh |
| `--depth` | 3 | Synthetic camera Z in meters, 0.1–100 |
| `--fov` | 45 | Assumed vertical FOV in degrees, 10–120 |

Inputs are single-frame JPEG/PNG, at most 128 MiB and 40 megapixels. EXIF orientation is applied before analysis; embedded ICC profiles convert to sRGB. Images without an ICC profile use the decoder's RGB interpretation as sRGB. Transparency is composited on white; this stage exports one opaque object. The output image retains its aspect ratio.

In the Viewer, use **Frame all** if the image is cropped by a narrow viewport. Expand `Dummy Image Surface` down to the primitive to edit its material. The surface has no back-side geometry, so it disappears when viewed from behind. Its original photo shading is already in the placeholder base color; the final PBR render is not a reference-matching result. Render Mode → Albedo isolates the input color map. The analysis normal EXR is not a material normal map, so Normal Strength has no visible effect on this Dummy material.

## Modules and contracts

```text
reconstruct.py                     CLI and composition
pipeline/
  types.py                         Shared typed arrays and result validation
  image_io.py                      Decode / EXIF / ICC / resize
  depth_backend.py                 DepthBackend → DummyDepthBackend
  normal_backend.py                NormalBackend → DummyNormalBackend
  segmentation_backend.py          SegmentationBackend → DummySegmentationBackend
  material_backend.py              MaterialBackend → DummyMaterialBackend
  geometry_builder.py              Camera-Z backprojection / regular grid
  gltf_writer.py                   RH glTF 2.0 GLB export
  auxiliary_writer.py              Float32 EXR / label PNG / previews
  scene_exporter.py                Reuses scene_package.py and shared schema
  runner.py                        Dependency-injected orchestration
```

Every backend exposes `predict(image: InputImage)`. Input is a read-only H×W×3 uint8 sRGB array with top-left origin. Outputs:

- Depth: H×W float32 positive camera Z; Dummy fills 3 meters.
- Normal: H×W×3 float32 signed unit vectors; Dummy fills (0,0,-1), facing the camera.
- SegmentationBackend: CPU boolean mask proposals and optional IDs/names/categories. SceneDecomposer resolves overlaps into H×W uint32 region labels and per-region records; Dummy supplies one full-image region.
- Material: input RGB as placeholder base color; roughness 0.8, metallic 0.

Camera/world space is left-handed, +Y up, +Z forward; the camera is at the origin. GLB writing reflects Z and reverses winding, because glTF is right-handed. The existing C++ importer converts it back. Point-map positions and mesh UVs sample the same pixel centers. All prediction arrays must match the processed image shape and use finite values; normals must be unit length on valid pixels. Geometry validity and segmentation are separate: a pixel can belong to a region without having valid depth.

Model-specific normalization, inverse-depth conversion, resizing and coordinate conversion belong in the geometry adapter. The CLI supports Dummy / MoGe geometry and Dummy / SAM 2 segmentation independently. SceneDecomposer splits the shared geometry into non-overlapping logical objects without changing point positions or UVs.

## Output

```text
scene01/
  scene.json                       Shared ScenePackage v1, same schema as C++
  meshes/scene_mesh.glb             Indexed grid with embedded photo texture
  textures/base_color.png           Processed sRGB image
  masks/segmentation.png            RGB bytes encode ID = R + 256G + 65536B
  debug/
    depth.exr                      Z channel, float32 camera Z
    normal.exr                     RGB = signed camera XYZ, float32
    pointmap.exr                   RGB = world XYZ, float32
    depth_preview.png
    normal_preview.png
    segmentation_preview.png
    reconstruction.json            Dummy marker, source hash, dimensions, versions
```

EXR files are real losslessly ZIP-compressed float32 EXRs. Tests read them back numerically. C++ ScenePackageLoader still treats auxiliary files as opaque references; the renderer displays the **GLB and PNG**, not the EXRs. Previews are visualizations and must never replace the numeric files. All package references are relative; the package can be moved intact. The schema is still `schemas/scene-package.schema.json`, not a new Python-only format.

## Verify

```powershell
# From the repository root:
.\tools\reconstruction\.venv\Scripts\python.exe tools/reconstruction/test_reconstruction.py --validator "build/Debug/ValidateScenePackage.exe" --validator "build/Release/ValidateScenePackage.exe"
.\tools\reconstruction\validate.ps1
# CPU/format/C++ loading only:
.\tools\reconstruction\validate.ps1 -SkipWindows
```

Tests cover orientation, alpha, ICC, backprojection, winding, GLB layout, EXR round trips, backend injection, Unicode paths, working-directory independence, deterministic export and preservation of existing output. The optional C++ validators actually decode the generated assets. `validate.ps1` additionally opens finite Debug/UI/material-edit/Release/WARP renderer runs.

Create an asymmetric test JPEG if you have no input handy:

```powershell
.\tools\reconstruction\.venv\Scripts\python.exe tools/reconstruction/test_reconstruction.py --make-input "generated/reconstruction-input.jpg"
.\tools\reconstruction\.venv\Scripts\python.exe tools/reconstruction/reconstruct.py "generated/reconstruction-input.jpg" --output "generated/my-dummy-scene"
```
