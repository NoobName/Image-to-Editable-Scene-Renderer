# Geometry estimation: MoGe-2 and Dummy

`GeometryEstimationBackend.predict(InputImage)` returns CPU NumPy arrays. Model code, PyTorch, checkpoint loading and OpenCV coordinate conversion are isolated in `pipeline/adapters/moge.py`. The renderer and mesh/export stages do not import MoGe. Segmentation and material inference remain Dummy.

## Selection and fixed versions

The [official MoGe repository](https://github.com/microsoft/MoGe) now includes MoGe-3 with additional FlexGEMM/Triton dependencies. This Windows integration uses the smaller official MoGe-2 normal model and a fixed v2 source revision, installing only inference dependencies.

- Code: `microsoft/MoGe`, commit `b942f00bdc2a2a23ebb474fbe034d487e6dcceec`.
- Geometry helper: upstream-specified `EasternJournalist/utils3d`, commit `3fab839f0be9931dac7c8488eb0e1600c236e183`.
- Weights: [Ruicheng/moge-2-vits-normal](https://huggingface.co/Ruicheng/moge-2-vits-normal), revision `26b477f41595707c5db6770294c0d1721e8ed4ed`, 140,550,416 bytes.
- Checkpoint SHA256: `79a16621928c2bf0ed04659218c55c01075e950507f40bb3332fb4c873d3e1dc`.

Source archives are SHA256-verified and retain upstream licenses. The source and model card identify MIT licensing. Downloaded source/weights remain in ignored local directories. Inference dependencies are torch, scipy, OpenCV headless and huggingface-hub, plus the base NumPy/Pillow/OpenEXR environment. No training tools, Gradio, torchvision, Triton or system CUDA toolkit are installed. CUDA PyTorch includes its runtime but needs a compatible NVIDIA driver.

## Install and run

### Dedicated Miniconda environment (Windows CUDA)

The project environment is named `image-scene-renderer` (Python 3.13.7, PyTorch 2.8.0+cu129). On this computer it lives at `D:\miniconda\envs\image-scene-renderer`. Existing NV00 and other environments are independent. The compatible CUDA wheel was reused from the local pip download cache; the checkpoint cache remains in this repository.

```powershell
# In Miniconda Prompt, from the repository root:
conda activate image-scene-renderer
python tools/reconstruction/reconstruct.py "input.jpg" --output "generated/my-cuda-scene" --geometry-backend moge --device cuda --max-size 512 --grid-size 129
.\build\Debug\ImageSceneRenderer.exe --package "generated/my-cuda-scene"

# For a NEW installation only:
conda env create -f tools/reconstruction/environment.yml
conda activate image-scene-renderer
python tools/reconstruction/fetch_moge_sources.py

# Use the activated interpreter with the validation scripts:
.\tools\reconstruction\validate.ps1 -Python python -SkipWindows
.\tools\reconstruction\validate-moge.ps1 -Python python -Device cuda -Offline
```

If activation is unavailable in a particular shell, use `conda run --no-capture-output -n image-scene-renderer python tools/reconstruction/reconstruct.py ...`. Do not nest the old `.venv` activation inside Conda. `python -c "import sys; print(sys.executable)"` identifies the selected interpreter.

`environment.yml` specifies this CUDA configuration and the same direct pipeline dependencies as the requirements files. Keep their versions in sync when upgrading. The two validation scripts accept `-Python` to select an interpreter; omitting it retains their original `.venv` behavior. No changes to system CUDA, drivers, shell initialization or existing environments are required.

### Existing local venv alternative

From the repository root:

```powershell
.\tools\reconstruction\setup-moge.ps1 -TorchBackend cpu
# Alternatively switch this project venv to CUDA 12.8:
.\tools\reconstruction\setup-moge.ps1 -TorchBackend cu128

.\tools\reconstruction\.venv\Scripts\python.exe tools/reconstruction/reconstruct.py "input.jpg" --output "generated/my-geometry" --geometry-backend moge --device auto --max-size 512 --grid-size 129
.\build\Debug\ImageSceneRenderer.exe --package "generated/my-geometry"
```

Only one PyTorch build occupies the venv at a time; the exact `+cpu` / `+cu128` requirement allows switching. CPU wheels can be hundreds of MB; CUDA wheels are several GB. First inference downloads the small normal checkpoint into `.cache/huggingface/` under this directory. Input images are processed locally and are not uploaded.

The official checkpoint loads with `weights_only=True`, strict state matching, eval and inference modes. No optimizer or training runs. `--checkpoint "path/model.pt"` accepts a local official MoGe-2 normal checkpoint; its hash is recorded but not checked against the default small model hash. v1/v3, arbitrary architectures, or missing metric/normal heads are rejected.

`--offline` prohibits weight downloads and requires a cache/local checkpoint. Missing dependencies, invalid results or CUDA exhaustion fail explicitly; the CLI never silently substitutes Dummy. Output must be new or empty. User paths are relative to the current working directory, including when running directly from `tools/reconstruction/`.

MoGe input edges must both be at least 16 pixels and aspect ratio must be within 1:2–2:1. Shared decoding applies EXIF orientation, ICC conversion, resize and alpha compositing before inference.

## Controls

| Option | Default | Meaning |
| --- | --- | --- |
| `--geometry-backend` | dummy | dummy / moge |
| `--device` | auto | CUDA if available, otherwise CPU; explicit CUDA fails if unavailable |
| `--resolution-level` | 0 | Model token budget 0–9; higher costs more time/memory |
| `--checkpoint` | pinned model | Optional local MoGe-2 normal checkpoint |
| `--offline` | off | Cached/local weights only |
| `--max-size` | 1024 | Longest processed/output image edge |
| `--grid-size` | omitted | Stage 11: every valid pixel; explicit 2–257 selects coarse sampling |
| `--confidence-threshold` | 0.5 | Minimum confidence; validity mask always enforced |
| `--depth-edge-threshold` | 0.15 | Maximum `(maxZ-minZ)/minZ` per triangle |
| `--depth`, `--fov` | 3, 45 | Dummy-only assumptions; MoGe estimates geometry and camera |

Image size, encoder token budget and mesh density are independent. Lowering only max-size does not proportionally reduce inference cost. Since MoGe confidence is binary, thresholds in (0,1] behave identically. Conservative filtering may leave holes; raising the depth threshold may introduce stretched faces at foreground/background boundaries.

## Unified contract

- `depth`: float32 H×W camera-forward Z, zero invalid.
- `normal`: float32 H×W×3 signed camera-space unit vectors on valid pixels, zero invalid.
- `point_map`: float32 H×W×3 camera-space positions; Z agrees with depth, zero invalid.
- `camera_intrinsics`: float32 3×3 normalized top-left UV matrix, image +Y down. Pixel centers are `(column+0.5,row+0.5)`; multiply rows 0/1 by width/height for pixel units.
- `confidence`: float32 H×W in [0,1], zero invalid. The official MoGe infer mask becomes **binary validity, not calibrated geometry confidence**.
- `valid_mask`: bool H×W; authoritative regardless of confidence threshold.
- `scale_type`: metric / relative / synthetic; metadata records provenance and confidence meaning.

Points/normals use LH camera coordinates (+X right, +Y up, +Z forward); the adapter reflects OpenCV Y. GLB export separately reflects Z and reverses winding for glTF. MoGe runs with `force_projection=True`. GeometryBuilder consumes returned point samples and estimated K rather than recomputing them using Dummy FOV.

ScenePackage v1 supports a centered symmetric camera and square pixels. Unsupported principal point/skew/aspect is rejected explicitly. A relative-scale backend needs explicit scale conversion before meter-based export. Future models implement this contract without imitating MoGe's API.

## Debug files and limits

Stage 13 adds [Marigold intrinsic material estimation](Material.md). Geometry confidence/normal arrays below remain separate from material confidence/tangent normal maps. New reconstruction defaults to neutral material; add `--material-backend marigold` for estimated PBR maps or use Original Image view to inspect the source.

```text
debug/depth.png           percentile 2–98 grayscale preview, invalid black
debug/normal.png          signed camera normal mapped to RGB, invalid black
debug/confidence.png      binary validity preview for MoGe
debug/valid_mask.png      authoritative validity
debug/confidence.exr      full float32 confidence
debug/geometry.npz        five requested arrays plus valid_mask
debug/camera.json         normalized/pixel K, coordinates, FOV, preview bounds
debug/reconstruction.json model revision, weight hash, device, scale and limitations
```

Original EXRs and `*_preview.png` names remain. Previews must not replace numeric data. C++ renders GLB/PNG and merely retains auxiliary file references; no model-specific schema change or Python runtime is needed.

Stage 11 mesh generation keeps every eligible pixel as a vertex and filters triangles by validity, depth span, 3D edge stretch/length, area and degeneracy. Isolated eligible vertices remain unreferenced. Explicit coarse grids additionally inspect interior invalid pixels and depth edges. See [Geometry.md](Geometry.md) for all controls and offline remeshing. Stage 12 adds [object segmentation](Segmentation.md) over this same geometry. It reconstructs visible surface only, without hidden surfaces or watertight completion. Base color still includes the photo's lighting. Final PBR rendering need not match the input; use Albedo to inspect colors/orientation, Normal to inspect shape, and Frame all for narrow UI viewports. Normal Strength has no effect without a tangent-space normal texture.

## Verify

```powershell
# Tests need no models or torch imports:
.\tools\reconstruction\.venv\Scripts\python.exe tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
# Official indoor example: real inference, numeric checks, C++ readers and DX12 windows:
.\tools\reconstruction\validate-moge.ps1 -Device cpu
# Reuse cached weights, omit GPU windows:
.\tools\reconstruction\validate-moge.ps1 -Device cpu -Offline -SkipWindows
.\tools\reconstruction\.venv\Scripts\python.exe tools/reconstruction/inspect_geometry.py generated/my-geometry --require-model
```

Real validation creates a fresh GUID output. Unit tests use explicit model-output fixtures; they are separate from actual inference/window validation. The official example image remains under ignored `.vendor/` and generated assets remain local.
