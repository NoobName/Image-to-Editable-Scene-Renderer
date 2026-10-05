# AnalysisMaps v1 (Prompt 18)

An optional, strict `analysis/analysis.json` sidecar connects saved numeric predictions to image-domain DX12 resources. It does not change `scene.json` v1 or `relighting/relighting.json`, add lighting, or alter Source/3D Final. The shared schema is `schemas/analysis-maps.schema.json`; Python and C++ also check semantic and numeric invariants.

## Run

From the project root, in the existing `image-scene-renderer` Conda environment:

```powershell
python tools/reconstruction/export_analysis.py generated/scene13 --output generated/scene18-reader
.\build\Debug\ImageSceneRenderer.exe --package generated/scene18-reader --work-mode image --image-view geometry-normal
```

The destination must be new or empty. This reads retained geometry/material arrays and segmentation assets, copies the old package, adds runtime maps and a truthful source anchor if needed, validates, then atomically publishes. No model is run. Old EXR/NPZ, meshes, textures and scene.json bytes are preserved. A legacy processed image remains low resolution, with source-camera calibration required; retained analysis intrinsics are separately identified.

New `reconstruct.py`, remesh, segmentation and material exports generate these maps from current arrays as part of the existing staging transaction. They preserve the source anchor; they do not run the model at anchor resolution.

## Contract and formats

Top-level metadata includes version, stable sourceId, source/analysis sizes, sourceToAnalysis, edge-origin half-integer centers, normalized/pixel intrinsics, immutable reconstruction camera-to-world, scale type, and sampling policy. Initially the reconstruction world equals the LH camera frame; arbitrary world transforms are rejected. Editable scene.camera/entities are not a source of these values.

Every map is either `available` (relative path, format, size, space, units, validity, sampling, provenance, range) or `unavailable` with a nonempty reason. All 13 keys must be present:

| Key | DDS format | Meaning / validity |
| --- | --- | --- |
| depth | R32_FLOAT | positive camera Z, meters/relative-units/synthetic-units; geometry validity |
| normal, normalWorld | RGBA32_FLOAT | signed unit XYZ in LH camera / identity reconstruction world; geometry validity |
| position | RGBA32_FLOAT | camera XYZ, same scale as depth; geometry validity |
| validity | R32_UINT | exactly 0 or 1 |
| region | R32_UINT | discrete label ID; 0 is unassigned |
| albedo | RGBA32_FLOAT | linear RGB reflectance [0,1], not source RGB |
| roughness, metallic | R32_FLOAT | [0,1] |
| geometryConfidence | R32_FLOAT | backend quality [0,1]; MoGe adapter currently supplies binary validity |
| materialConfidence | R32_FLOAT | retained estimate quality [0,1]; does not certify fallback normal |
| regionConfidence | R32_FLOAT | proposal score [0,1]; validity is nonzero region |
| tangentNormal | RGBA32_FLOAT | material tangent-space unit vector; current Marigold normal is flat fallback |

All vector payload W channels are zero padding. Invalid geometry is finite zero; validity is authoritative. The RGBA32F sampling-test output uses W as **sampling validity**, a different contract from file padding. Float sampling-test output is not an integer label transport; raw R32_UINT readback preserves all 32 label bits.

Runtime DDS is a deliberate project subset, not a general DDS importer: little endian, 148-byte DDS+DX10 header, one 2D slice/mip, no compression, exact pitch and byte count, no trailing bytes. Format codes are 41/2/42 respectively. Dimensions are 1..2048 per edge, at most 4,194,304 texels, file at most 64 MiB+148 bytes, total map files at most 256 MiB. Exceeding a limit fails; it never shrinks the anchor. NaN/Inf in any texel, including invalid ones, is rejected. JSON/path/schema, data range, unit normals, intrinsics scaling, source mapping and pinhole positions are checked in both languages.

For analysis pixel (i,j), `u=(i+.5)/W`, `v=(j+.5)/H`. With normalized `fx,fy,cx,cy` and camera Z:
`P=((u-cx)*Z/fx, -(v-cy)*Z/fy, Z)`; `Kpixels=diag(W,H,1)*Knormalized`. Image Y points down; camera Y points up. Source mapping is the validated phase-16 resize mapping.

## GPU and UI

`SourceObservation::analysisMaps` holds validated CPU bytes. `AnalysisTextures` owns typed numeric textures separately from PBR textures. Startup and `PreparedScene` upload them with the anchor and scene; commit/retirement use the existing fence mechanism. Missing sidecar is compatible; malformed sidecar rejects the candidate before publication. There is no Torch/model dependency in C++.

Image View accepts `original`, `grid`, `depth`, `geometry-normal`, `world-normal`, `position`, `validity`, `region`, `albedo`, `roughness`, `metallic`, `geometry-confidence`, `material-confidence`, `region-confidence`, `tangent-normal`. Existing `--render-mode` still selects 3D views. In Image Mode the combo is independent of 3D edits and Look. The Inspector shows units, provenance, range, immutable cameras, sizes and mapping. Missing data shows a striped unavailable view and an explicit reason. Invalid geometry appears purple; it does not cut holes in Source.

Labels/validity use nearest integer Load. Continuous data use bilinear interpolation only when all four taps are valid and share the center's region; geometry also requires relative depth difference <= 0.15. Otherwise the valid center is used; an invalid center remains invalid. Interpolated normals are normalized. This conservative policy is not edge completion, a geometric super-resolution method, or anti-aliasing for severe downscaling.

Albedo is encoded from linear RGB for display; numeric data are visualized without ACES/Bloom/Look. Source keeps its phase-17 single sRGB decode/encode. Position colors use a shared global component range and are diagnostic, not metric RGB colors.

## Verification

```powershell
.\tools\Build.ps1 -VisualStudioPath D:\VisualStudio
python tools/reconstruction/analysis_examples.py generated/prompt18-reader
.\build\Debug\AnalysisGpuTests.exe generated/prompt18-reader/step generated/prompt18-reader-gpu
python tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
.\tools\Validate-Analysis.ps1
python tools/reconstruction/verify_analysis.py
```

The last two commands use this workstation's existing `generated/prompt16`, `generated/prompt18`, `generated/scene13` and exported `generated/scene18`; they are validation fixtures, not tracked assets. See the script parameters for alternate roots. GPU tests default to WARP; a fourth `--hardware` argument selects hardware. Captures write `*-maps/*.bin` and `readback.json`: payloads are byte-compared to validated CPU data and states record SRV(128) → COPY_SOURCE(2048) → SRV(128). RGBA32F sampling tests also cover RT(4) → COPY_SOURCE → RT(4). The fence completes before CPU Map. Capture intentionally blocks and is not performed every interactive frame.

No fresh real-model inference, relighting, normal-detail prediction, or hidden-surface completion is performed in phase 18. Model provenance and fallback limitations remain in the copied debug reports.
