# Stage 11: visible-surface grid mesh

Depth + calibrated camera, or an existing point map, becomes a textured triangle mesh. This is deterministic geometry processing: no model is loaded by the builder, and no missing surfaces are synthesized. Python remains independent of DX12.

## Run from the repository root

```powershell
conda activate image-scene-renderer
python tools/reconstruction/reconstruct.py "input.jpg" --output generated/new-scene --geometry-backend moge --device cuda --max-size 512 --offline
.\build\Debug\ImageSceneRenderer.exe --package generated/new-scene --render-mode albedo
```

`--offline` requires the already cached MoGe source and weights; see [MoGe.md](MoGe.md) for setup. Dummy remains available with `--geometry-backend dummy`, but its constant depth cannot demonstrate different near/far parallax.

To experiment with saved predictions without inference:

```powershell
python tools/reconstruction/remesh.py generated/scene10-cuda --output generated/new-point-mesh
python tools/reconstruction/remesh.py generated/scene10-cuda --output generated/new-depth-mesh --geometry-source depth
python tools/reconstruction/remesh.py generated/scene10-cuda --output generated/new-coarse-mesh --grid-size 129
```

Remeshing reads `debug/geometry.npz`, `debug/reconstruction.json` and `textures/base_color.png` from a stage-10+ package. The first route consumes XYZ samples directly; the depth route recomputes XYZ using the saved intrinsics. Prediction normals and confidence are preserved. Input data stays unchanged. Output must be new or empty. The input image is already processed; remeshing cannot recover discarded image resolution. Custom edited materials are not copied: this command reconstructs the stage-10 photo surface.

## Mesh controls (shared by both CLIs)

| Option | Default | Effect |
| --- | --- | --- |
| `--grid-size` | omitted | Every eligible processed pixel is a vertex; explicit 2–257 selects a coarse grid |
| `--confidence-threshold` | 0.5 | Keep pixels with valid mask AND confidence ≥ threshold |
| `--depth-edge-threshold` | 0.15 | Triangle depth span / minimum depth must not exceed this |
| `--depth-edge-meters` | 0 | Additional absolute depth span limit; 0 disables |
| `--max-edge-stretch` | 8 | Every 3D edge must be ≤ this × its projected image footprint at nearer depth |
| `--max-edge-meters` | 0 | Additional absolute 3D edge length limit; 0 disables |
| `--max-triangle-area` | 0 | Additional triangle area limit in square meters; 0 disables |

Limits combine with AND. Raise limits cautiously: they may reconnect foreground to background. Lower limits may remove valid steep surfaces. MoGe currently supplies binary validity as confidence, not a calibrated accuracy estimate. Confidence 0.5 cannot eliminate every geometric error.

Each cell tries both diagonals, chooses the split retaining most valid triangles, then the shorter diagonal on ties. Each triangle checks its own three vertices; one invalid corner need not discard the other valid half-cell. Coarse grids additionally reject rectangles containing invalid pixels or interior horizontal/vertical depth discontinuities, even when grid samples miss them. This conservative coarse rule can remove more geometry; use the default pixel mode for fidelity.

All eligible vertices are retained, including isolated vertices. Invalid pixels produce no vertices. Isolated vertices produce no visible fragments because they have no indices. A completely empty triangle set is an explicit error, as is an image narrower than two pixels. No quad is invented from a single pixel.

Candidate processing uses 64 cell rows per batch. Input is limited to 2048 pixels per edge; final mesh and GLB still use O(W×H) memory. Start at `--max-size 512`. This controls processed image and geometry resolution; `--grid-size` controls only mesh sampling.

## Coordinates and export

Pixel centers use `u=(x+0.5)/W`, `v=(y+0.5)/H`. In normalized intrinsics, `X=(u-cx)Z/fx`, `Y=-(v-cy)Z/fy`. Depth means forward Z, not Euclidean distance. Direct point-map mode preserves supplied XYZ. Current ScenePackage camera supports centered principal point and square pixels; unsupported calibration or unresolved relative scale fails explicitly.

Mesh vertices and normals are LH, +Y up, +Z forward. UV origin is top-left. Export reflects Z for glTF's RH convention and reverses winding. The importer performs the inverse conversion. The texture is the processed sRGB input, with no lighting removal. The half-pixel border is intentionally not extrapolated into new geometry.

New packages contain:

```text
scene.json
meshes/scene_mesh.glb      # positions, normals, UVs, uint32 indices, embedded PNG, material
textures/base_color.png   # same PNG retained separately for inspection/remeshing
masks/segmentation.png
debug/geometry.npz        # numeric prediction arrays, not mesh buffers
debug/reconstruction.json # counts, thresholds, rejection reasons, source metadata
debug/depth.exr, normal.exr, pointmap.exr, confidence.exr
debug/depth.png, normal.png, confidence.png, valid_mask.png
```

Report `pipeline_version=3`; interchange schema remains **ScenePackage v1**. Existing old packages with `image_surface.glb` still load. New packages omit the Object material override so the embedded GLB photo material survives. GLB uses a linear-filtered clamp sampler and glTF Base Color sRGB semantics. A standalone `--model generated/new-scene/meshes/scene_mesh.glb` works, but auto-frames it instead of using the predicted camera.

`geometry.mesh_filtering` records candidate count, mutually exclusive rejection counts in invalid/depth/edge/area/degenerate order, isolated vertices and thresholds. Candidate count equals accepted triangles plus rejected triangles for the selected diagonals. The four trial triangles per cell are not double-counted.

## Inspect in the renderer

Choose **Wireframe**, **Depth**, **Normal**, or **Albedo** from the Viewport menu, or use `--render-mode wireframe|depth|normal|albedo`. Wireframe selects `D3D12_FILL_MODE_WIREFRAME` PSOs, preserves culling/depth behavior, and uses an unlit green color. It does not change the shadow-map PSO. Debug views bypass look effects. Normal displays interpolated shading normals (including a material normal map if present); it is not a per-face geometric-normal measurement.

Depth displays linear camera Z normalized by near/far, with white background. New package far = `max(1, 2 × maximum valid depth)`, giving room for modest camera movement and readable depth contrast. If moving far back clips the scene, increase Far in the Camera Inspector. EXR/NPZ preserve original depth values regardless of display normalization.

Use small A/D movements with the pointer over the image to observe depth-dependent parallax. Right-drag rotates the camera. Large moves expose holes behind foreground objects; these are expected missing observations. Frame all changes pose and breaks exact photo-view alignment. Dense wireframes look filled when triangle size approaches a screen pixel; zoom in or create a coarse diagnostic mesh.

## Reproduce checks

```powershell
.\tools\Build.ps1
python tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
python tools/reconstruction/inspect_geometry.py generated/new-scene --require-model
.\tools\Run-Smoke.ps1 -Package generated/new-scene -RenderMode wireframe -Capture -LogName mesh-wire
.\tools\Run-Smoke.ps1 -Package generated/new-scene -RenderMode depth -Capture -LogName mesh-depth
.\tools\Run-Smoke.ps1 -Package generated/new-scene -RenderMode normal -Capture -LogName mesh-normal
```

An analytic fixture verifies real GPU parallax rather than merely checking that images differ:

```powershell
python tools/reconstruction/verify_mesh_rendering.py prepare generated/my-parallax
.\tools\Run-Smoke.ps1 -Package generated/my-parallax/base -RenderMode albedo -Frames 90 -Capture -LogName parallax-base
.\tools\Run-Smoke.ps1 -Package generated/my-parallax/translated -RenderMode albedo -Frames 90 -Capture -LogName parallax-shift
python tools/reconstruction/verify_mesh_rendering.py compare generated/parallax-base.bmp generated/parallax-shift.bmp
```

At 1280×720, normalized fy=1 and horizontal camera translation 0.1 m, the 2 m patch shifts -36 pixels and the 4 m patch -18 pixels. The checker requires ±1 pixel agreement and the expected disparity ratio. All commands are file-based; no Python/DX12 runtime binding is introduced.
