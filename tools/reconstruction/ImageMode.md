# Stage 17: independent source image view

The renderer supports two independent work modes. `scene` keeps the existing 3D editor and Final render path. `image` displays the validated source anchor with aspect-preserving fit. The UI calls the second mode **Image Relighting**, but this stage computes no new lighting.

## Usage

From the project root, after generating a Stage 16+ package:

```powershell
.\build\Debug\ImageSceneRenderer.exe --package "generated/my-anchor16" --work-mode image
.\build\Debug\ImageSceneRenderer.exe --package "generated/my-anchor16" --work-mode image --image-view grid
.\build\Debug\ImageSceneRenderer.exe --reconstruct "input.jpg" --reconstruction-preset dummy --work-mode image
```

The UI retains the three existing columns. Viewport `Mode` selects **3D Scene** or **Image Relighting**. Image mode offers **Original View**, **Pixel Grid** and automatic Fit; Inspector shows package, source/analysis/viewport dimensions, image rectangle, immutable source camera, mapping and source identity. Grid spacing is 64 source pixels; hover coordinates use edge-origin half-integer pixel centers. No pan/zoom is implemented.

The old `--render-mode` accepts exactly the existing values and preserves enum values. `original` still means the processed image on 3D geometry; its UI label is now **Original Image on Geometry**. It can have mesh holes and camera parallax. Independent Original View samples `source_anchor.png`, bypasses Look/HDR/tone mapping, and remains visible after hiding every 3D object. Switching back restores the free-camera pose/FOV and 3D debug selection; aspect follows the current 3D viewport.

Absent extension means 3D remains available and the Image button is disabled. An explicitly requested Image mode waits for a future validated source if none is initially available. A validated `legacy-processed` anchor is displayable with its honest low-resolution status, even when camera calibration is unavailable. A damaged present extension remains a load error. ImGui's bundled font may show missing Chinese glyphs in path labels; path handling and UTF-8 metadata still work.

## Data and resource contract

No change to scene.json v1 or the independent sidecar schema. `ScenePackageLoader` retains package root, anchor metadata, the verified WIC RGBA8 snapshot, auxiliary paths, and the optional `debug/reconstruction.json` report. A malformed optional debug report is ignored with an explicit diagnostic; it is not a required v1/anchor contract and must not prevent legacy scene loading. A malformed relighting sidecar still fails the load. Hashing and decoding use the same byte snapshot. `SourceObservation` and its pixels are shared through const pointers, separately from mutable Scene/Material/Camera.

Startup uploads Scene and source before publishing. Asynchronous `PreparedScene` uploads both into its own heaps/resources; `CommitPreparedScene` publishes its SourceObservation in the same main-thread transaction as the CPU Scene. Failures and cancellation before commit preserve the current document. Old scene/source resources and heaps retire only after the graphics fence. Upload staging buffers are released after the upload fence. This does not provide recovery from process-wide allocation failure or device removal.

The source SRV is `R8G8B8A8_UNORM_SRGB`, on a typeless single-mip texture. Sampling performs one sRGB decode; `SourceImage.hlsl` calls the existing `LinearToSrgb` once into an UNORM target. ImGui also samples the encoded viewport through UNORM, without another decode. Alpha is opaque because the Stage 16 canonical image already follows the transparency-compositing rule. Native-size RGB8 output must differ by at most 1 LSB from canonical RGB8. Strong minification may alias with the current one-mip linear filter; pixel equality is specified only at native size without the grid overlay.

## Reproducible validation

```powershell
.\tools\Build.ps1 -VisualStudioPath D:\VisualStudio
conda activate image-scene-renderer
# Only when these fixtures do not exist; refuses a nonempty output directory.
python tools/reconstruction/anchor_examples.py generated/prompt16
.\tools\Validate-ImageMode.ps1
python tools/reconstruction/verify_image_mode.py
python -X utf8=1 tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
```

`Validate-ImageMode.ps1` runs 19 finite-frame real swap-chain windows: old Final, fit, native Debug/Release/WARP, input isolation, repeated mode switches plus 3D edits, Look isolation, wide/narrow, UI grid, absent/legacy anchors, native ImGui handoff, EXIF orientation, failed/cancelled GPU preparation, failed/cancelled Python reconstruction, and two different successful background inputs. The last three reconstruction cases require the existing project Python environment. Expected reconstruction-error application exit code is 2; wrappers require it explicitly. Debug wrappers require `errors=0 warnings=0`.

`verify_image_mode.py` consumes BMP/readback and `.view.json` diagnostics, checks finite metadata, compares 13 image pairs and writes `generated/prompt17-results.json`. The old-Final regression comparison also requires `generated/prompt17-before.bmp`, captured with the pre-17 executable in this workspace. On a fresh checkout that historical baseline is unavailable; do not manufacture it with the new binary. The other GPU runs and CTests can run independently; explicitly record the missing baseline instead of claiming regression equivalence.

Additional test-only flags: `--window-size WIDTHxHEIGHT` (1..4096 each), `--fixed-size` (skip smoke resizing), `--image-smoke input|cycle|transaction` (requires a finite scene run). `--reconstruction-next-image path` chooses a different second input for repeated reconstruction. `Run-Smoke.ps1` exposes WorkMode/ImageView/ImageSmoke/WindowSize/FixedSize. UI native comparison fixes the existing 1103×475 layout to a 512×341 viewport; its crop coordinates are an explicit layout test fixture, not a public API.

CPU/UI tests: `ImageSessionTests` covers fit, camera isolation, mode persistence and actual ImGui radio-button events. `SourceImageTests` runs WARP offscreen with odd dimensions, full-range RGB patterns, repeated resource draws and wide/narrow constant-color letterbox references. Actual GPU window captures and JSON reports are separate evidence from those tests. Automated events are not described as manual desktop interaction.

Local teaching materials remain under ignored `docs/`; generated fixtures, captures and model caches remain untracked.
