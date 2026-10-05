# Object segmentation and scene decomposition

Stage 13 adds [intrinsic material estimation](Material.md). New reconstruction defaults to neutral material unless `--material-backend marigold` is selected. Existing stage-12 photo-material packages remain readable; `segment.py` and `remesh.py` preserve estimated maps when present.

Stage 12 turns one visible-surface mesh into independently editable regions. `SegmentationBackend.predict(image)` returns CPU mask proposals; all SAM/PyTorch details stay in `pipeline/adapters/sam2.py`. `scene_decomposer.py` resolves overlaps, computes statistics and partitions existing geometry. Neither Renderer nor the geometry builder imports SAM.

## Setup

```powershell
conda activate image-scene-renderer
.\tools\reconstruction\setup-sam2.ps1
python -m pip check
```

The setup script targets this project's existing torch 2.8.0 environment and installs matching torchvision 0.23.0, hydra-core 1.3.2, omegaconf 2.3.0 and iopath 0.1.10. Default wheel backend is `cu129`; `-TorchBackend cpu` or `cu128` requires the matching already installed torch variant. It does not upgrade torch. The complete CUDA environment spec is `environment.yml`; original Dummy `.venv` remains separate.

Official [SAM 2](https://github.com/facebookresearch/sam2) sources are pinned at `2b90b9f5ceec907a1c18123530e92e794ad901a4`. The [SAM 2.1 Hiera Tiny checkpoint](https://huggingface.co/facebook/sam2.1-hiera-tiny) is pinned at `de431c4043854a71d8101e17995dfe596bf101a5` and verified against SHA256 `7402e0d864fa82708a20fbd15bc84245c2f26dff0eb43a4b5b93452deb34be69` (156,008,466 bytes). Source archive hash is pinned too. Source and weights live under ignored `.vendor/` and `.cache/`. Official source YAML aliases are copied to regular local files during safe extraction; no Windows symlink privilege is needed.

Native Windows CUDA image inference was tested on the project's RTX 5070 Laptop, Python 3.13.7, torch 2.8.0+cu129 and torchvision 0.23.0+cu129. SAM's optional CUDA mask cleanup is disabled explicitly; no CUDA extension, training code or video server is built. The official [installation guide](https://github.com/facebookresearch/sam2/blob/main/INSTALL.md) recommends WSL for Windows; this tested image-only adapter is narrower in scope. CPU is selectable but slower. `--device cuda` fails clearly if CUDA is unavailable; it never silently substitutes Dummy.

SAM 2 predicts masks from points/boxes, not class names. The newer [SAM 3](https://github.com/facebookresearch/sam3#installation) supports text concepts and gated checkpoint access; this first integration uses the publicly downloadable Tiny checkpoint. Another model can implement the same CPU proposal interface later.

## Commands

Run from repository root, with the project environment activated:

```powershell
# Fully automatic masks + geometric heuristic categories
python tools/reconstruction/reconstruct.py input.jpg --output generated/my-scene12 --geometry-backend moge --segmentation-backend sam2 --device cuda --max-size 512 --offline

# Reuse saved geometry, run SAM only (defaults to --segmentation-backend sam2)
python tools/reconstruction/segment.py generated/scene11 --output generated/my-regions --segmentation-prompts tools/reconstruction/examples/indoor-prompts.json --device cuda --offline

# Rebuild a segmented mesh with different filters without running either model
python tools/reconstruction/remesh.py generated/my-regions --output generated/my-regions-remeshed

.\build\Debug\ImageSceneRenderer.exe --package generated/my-regions --render-mode albedo
```

`reconstruct.py` still defaults to **Dummy segmentation**. Explicitly choose `sam2` for real masks. Geometry and segmentation choices are independent. `--offline` requires cached weights. Outputs must be new or empty; existing scenes are preserved. Full MoGe + SAM CUDA inference at 512×341 was tested on the street example; high-resolution inputs and other GPU applications may require more memory. SAM internally encodes at its model resolution, so reducing mesh/image size alone does not proportionally reduce all SAM memory use.

Local generated examples (not committed):

| Package | Inference | Objects |
| --- | --- | --- |
| `generated/scene12` | Saved indoor MoGe + named SAM prompts | 9 |
| `generated/scene12-auto-final` | Saved indoor MoGe + automatic SAM masks | 21 |
| `generated/scene12-traffic` | Full CUDA MoGe + named SAM prompts | 8, including Building / Ground / Sky |

The example prompt files refer to MoGe's `01_HouseIndoor.jpg` / `03_Traffic.jpg` under the ignored pinned `.vendor/MoGe-.../example_images/` directory. Use new points/boxes for your own photograph. The example street image has no tree; the format accepts a user-named `Tree` region without pretending the example contains one.

## Named points / boxes

```json
{
  "version": 1,
  "regions": [
    {
      "id": "building-01",
      "name": "Building",
      "category": "major",
      "points": [[0.2, 0.3, 1], [0.7, 0.1, 0]],
      "box": [0.05, 0.05, 0.45, 0.85]
    }
  ]
}
```

Positions are normalized to `[0,1]` over the EXIF-oriented processed image. Points are `[x,y,label]`: 1 includes, 0 excludes. Boxes are `[left,top,right,bottom]`. Provide a positive point or box; either points or boxes may be omitted. Negative points help reject neighboring regions. The ID uses 1–64 ASCII letters/digits/underscore/hyphen and must be unique. `name` is a supplied display name, **not a text prompt passed to SAM**. Categories are `sky`, `ground`, `foreground`, `major`, `background`.

The adapter selects the highest predicted-IoU candidate per prompt. Prompt masks may overlap: earlier entries own overlapping pixels, so list small foreground objects before large walls/background. A later completely covered mask is discarded and counted in the report. Empty SAM masks are errors. A supplied prompt file must fit `--max-regions` with one slot reserved for remaining Background; excessive named prompts are rejected rather than silently truncated.

Without prompts, smaller masks win over containing masks; score and content hash deterministically break ties. Default `--min-region-pixels 64` removes tiny automatic regions, `--max-regions 32` limits regions including leftover Background (range 2–128). `--sam2-points-per-side 16` controls automatic sampling (4–32). Remaining pixels always receive a Background region. Automatic sky/ground/foreground labels use validity, image position, normals and depth; inspect them instead of assuming semantic accuracy.

## Format and geometry

```text
ScenePackage/
  scene.json
  objects/<stable-id>/
    region.json
    mask.png                 grayscale 0/255, full processed image size
    mesh.glb                 optional when retained triangles exist
  meshes/scene_mesh.glb       full diagnostic mesh, embedded original image
  textures/base_color.png    shared image used by region meshes
  masks/segmentation.png     label = R + 256*G + 65536*B
  debug/segmentation_overlay.png
  debug/segmentation_preview.png
  debug/reconstruction.json
  debug/geometry.npz          unchanged unified geometry arrays
```

Each manifest object has `id`, `name`, `region` and optional `mesh`. `region.json` shares `$defs.region` in `schemas/scene-package.schema.json` with both readers. It stores `id`, `name`, integer `labelId`, `category`, relative `mask`, `imageSize [W,H]`, `boundingBox [x,y,width,height]`, `pixelCount`, `validDepthPixels`, `averageDepth`, `namingSource`, `score`, `triangleCount`.

Bounding boxes are half-open pixel rectangles in the processed image. Average depth is arithmetic mean camera Z over `mask & valid_mask`, in geometry units (meters for the current export); it is `null` with no valid depths. A region can have a few valid pixels but no triangles. `score` is SAM mask-quality estimation, not probability of a semantic name or geometric correctness. `namingSource` is `prompt`, `geometry-heuristic`, `fallback` or `dummy`.

String object IDs are separate from contiguous raster label IDs. Named IDs persist through export/remesh; automatic IDs hash the original mask proposal and are deterministic for the same mask. They are not tracking IDs across different images/model predictions. Old v1 packages remain loadable without region metadata/IDs, with runtime `legacy:<name>` IDs. A region-only object is legal and creates a logical hierarchy node. Existing old packages need not have an `objects/` directory.

Every face must belong to a single region. Boundary faces are dropped; coarse grid faces also reject an interior region mismatch using a conservative bounding-box test. Vertex positions, normals and UVs come unchanged from the shared point map. No per-object recentering, inferred backsides or geometry completion is performed. Boundary cracks and previously existing depth holes remain visible.

Region GLBs reference `../../textures/base_color.png`; keep the whole package together. The normal loader only loads manifest object meshes, so the full diagnostic mesh is not drawn a second time. Each object has independent editable material factors while TextureManager shares the same image resource. A sky mask without geometry is still selectable for its statistics, but has no material surface to edit and is not an HDR environment map.

## Viewer and validation

Select a parent under **Objects**, then change Transform, **Visible** or material parameters. Visibility applies to its renderable descendants. Multiple distinct materials appear separately; edits stay with that object's material instances. The Region panel shows source statistics, which remain source-image measurements when the object is moved. Albedo reveals Base Color edits; Roughness/Final reveal roughness edits. Inspector changes are in memory only.

```powershell
python tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
.\build\Debug\ValidateScenePackage.exe generated/scene12/scene.json --objects
.\tools\Validate-Segmentation.ps1
```

The 47 model-free tests include mask validity, overlap priority, deterministic identity, invalid geometry statistics, dense/coarse mesh separation, shared texture/independent material instances, Python/C++ schema rejection and remeshing identity. `--objects` prints CPU-loaded hierarchy/material/texture evidence. Window validation uses explicit `--object-smoke chair`: hide at frame 30, restore at 50, change Base Color and roughness at 70; ordinary launches never perform these edits. GPU comparisons verify changed pixels stay in the projected chair mask (2 source-pixel contour tolerance), restoration matches exactly, HDR is finite, and Debug logs have zero errors/warnings. Tests exercise the same subtree/material logic; actual mouse clicking is a separate manual check.
