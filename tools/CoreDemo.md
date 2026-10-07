# Core demo and measurement contract (stage 32)

The demo is a bounded single-image editing system, not a complete inverse renderer.
Optional stages 33–35 are not prerequisites. Detailed Chinese lessons and machine-specific
evidence remain in ignored `docs/`; this file and the scripts are the portable handoff.

## Layers and ownership

```text
Original bytes → EXIF/ICC/alpha normalization → immutable canonical RGB8 anchor
                                        └→ analysis RGB (bounded resolution)
Python adapters → geometry / labels / material / intrinsic A,S,R
                → lighting fit / supported old-shadow analysis
                → versioned ScenePackage v1 + optional strict sidecars / DDS
CPU validation → SourceObservation + editable Scene → PreparedScene → fence-safe commit
  Image mode: fixed source camera → old/new shading → bounded confidence-weighted change
              → independent source-resolution composition → display / native export
  3D mode: editable mesh/material/free camera → PBR / shadow / IBL → HDR / artistic Look
Reference analysis → target proposal → optional bounded target optimization → DX12 check
Recipe: fixed source identity + source calibration + target/response/protection/display state
```

C++ has no Python runtime or model imports. Background Python is a finite process with
JSON progress and files. GPU submission publishes Scene and observation together; failed,
cancelled or stale work retains the previous session. Only derived data is copied into new
packages. Recipe never overwrites canonical pixels or saves arbitrary 3D editing.

Contracts: [anchor](reconstruction/AppearanceAnchor.md), [analysis](reconstruction/AnalysisMaps.md),
[lighting](reconstruction/LightingBaseline.md), [intrinsic](reconstruction/Intrinsic.md),
[recipe/export](RelightingRecipe.md), [reference](ReferenceLighting.md),
[optimization](LightingOptimization.md). Authoritative versions are in `schemas/`.
DDS is restricted single-mip DX10 R32_FLOAT/RGBA32_FLOAT/R32_UINT; previews never stand in
for numeric observations. Linear normal/position/depth and sRGB source have separate contracts.

## Reproduce without model downloads

Run in project-root PowerShell with the existing project Python environment activated:

```powershell
./tools/Build.ps1 -BuildDirectory generated/build-core
./tools/Run-CoreDemo.ps1 -BuildDirectory generated/build-core -Python python -Output generated/my-core-demo
./generated/build-core/Debug/ImageSceneRenderer.exe --recipe generated/my-core-demo/reference.json --work-mode image --ui
```

If Visual Studio is not discovered, pass its installation directory as `-VisualStudioPath`.
Use a **new output name** on each run. The generator creates analytic Lambert/GGX/shadow,
degenerate/black/missing-occluder fixtures and a 1500×1000 fine-text anchor. These are
project-authored diagnostics with explicit synthetic geometry/material/manual lighting,
not neural inference or photographs. `sources.json` records their hashes and roles.

The script runs real finite HWND windows, Debug/Release and WARP, exports PNG/DDS/metadata,
checks identity and replay, and writes `windows.json`/`verification.json`. ImGui event injection
is exercised separately by CTest; screenshots and CLI smoke are not manual desktop input.

## Real photo, fully offline once provisioned

Use a photo you have rights to use. Install dependencies/weights with the existing setup
instructions in [reconstruction README](reconstruction/README.md) before going offline.
The command below never downloads missing weights; missing cache is an error:

```powershell
python tools/reconstruction/profile_reconstruction.py input.jpg --output generated/my-real-demo --sizes 512 512 256 --real
./tools/Run-CoreDemo.ps1 -BuildDirectory generated/build-core -Python python -Output generated/my-real-evidence -RealPackage generated/my-real-demo/run-0-512/final
```

MoGe, SAM2, Appearance and Lighting adapters use pinned revisions and sequential release.
The script produces separate reconstruction/intrinsic/lighting/final packages; source bytes
and anchor survive every step. No model is trained. Omitting `--real` explicitly selects
Dummy/neutral/proxy and cannot establish geometry or lighting accuracy. First/repeat refers
to process/OS cache warmth; **models are reloaded**, not held in a service. A true disk-cold
boot is not simulated by deleting user caches.

## Demonstration order

1. Show photo provenance and canonical/analysis dimensions. Open the final package in Image mode.
2. Original Image is the fixed anchor; inspect Depth, Geometry Normal, Estimated Albedo,
   Estimated Original Shading, Calculated Old Shading, Confidence and Residual.
3. In Target Lighting, drag the sun. Show Calculated New Shading, Ratio and Relighted,
   then use side-by-side or wipe. Source calibration is a separate explicit action.
4. Switch to 3D Scene: free camera, mesh holes, selectable objects and material edits.
   Return to Image Relighting; source camera/anchor remain unchanged.
5. Load a Reference proposal, inspect camera-relative direction/confidence, then Apply.
   For a repeatable successful reference use the analytic `reference/source` package with
   `reference/reference-proposal`; an ambiguous real photo may correctly reject transfer.
6. Save Recipe, exit, open it again, Export native PNG and float diagnostic buffers.
   Float output is display-referred linear data, **not recovered HDR radiance**.
7. Show black/missing-occluder and specular stress cases. Preserving an unsupported region
   is a guardrail, not evidence of recovered hidden detail or glass transmission.

## Profiling without screenshots or export stalls

```powershell
./tools/Run-Smoke.ps1 -BuildDirectory generated/build-core -Configuration Release -Package generated/my-real-demo/run-0-512/final -WorkMode image -ImageView relighted -ImageSmoke profile-drag -WindowSize 1920x1080 -FixedSize -Frames 420 -Profile -LogName core-drag
./tools/Run-Smoke.ps1 -BuildDirectory generated/build-core -Configuration Release -Package generated/my-real-demo/run-0-512/final -WorkMode image -ImageView relighted -WindowSize 1920x1080 -FixedSize -Frames 420 -Profile -LogName core-idle
```

`generated/core-drag.profile.json` contains raw samples and nearest-rank p50/p95, not an FPS
average. First 60 frames are warmup. CPU measures the loop after message pump through Present,
including UI/jobs/fence pacing. GPU timestamps measure recorded graphics work, excluding
Present; imageUpdate measures shading/ratio/composition, excluding display/upload/AI.
Vsync stays enabled, so CPU frame time is not GPU execution time. Capture frames are excluded
from whole-frame distributions; keep profiling runs free of captures/exports/jobs anyway.
Samples are capped at 65,536. Timestamp readback reuses the three frame fences without a new
per-frame wait. Explicit final collection follows Flush.

DXGI `CurrentUsage` is sampled per process at frame ends, recording local/nonlocal high-water
and OS budget. It is not adapter-wide allocation, guaranteed sub-frame peak, or CUDA tensor
peak. Python reports synchronized stage time and CUDA peak allocated/reserved separately.
Startup logs separate CPU decode/validation and GPU/window initialization (also shaders/IBL).
Report background load, driver and configuration; 1080p ≤33 ms is a target, not a blanket claim.
Analysis size, source size, viewport and export size must always accompany a timing table.

Measured 2026-10-06: RTX 5070 Laptop (8151 MiB), driver 616.64, Ryzen 9 8945HX,
Windows build 26200. 420 frames/run, first 60 excluded, VSync enabled, no capture/export;
ordinary desktop background activity was not eliminated. Source/export 1500×1000.

| Release workload | Analysis | Window / viewport | CPU p50 / p95 ms | GPU p50 / p95 ms | Sampled process local MiB |
|---|---|---|---:|---:|---:|
| Idle | 512×341 | 1920×1080 / same | 4.162 / 4.219 | .102 / .114 | 470.14 |
| Continuous target drag | 512×341 | 1920×1080 / same | 4.160 / 4.221 | .736 / .755 | 470.14 |
| Drag with UI | 512×341 | 1920×1080 / 1329×923 | 4.161 / 4.209 | .787 / .814 | 437.37 |
| Drag, lower analysis | 256×171 | 1920×1080 / same | 4.162 / 4.207 | .555 / .572 | 337.57 |

At 512, image-update-only GPU p95 was .674 ms. Full offline four-adapter analysis plus
export took 33.09 s first / 24.77 s repeat; 256 took 19.00 s. Highest measured CUDA
allocated/reserved peaks were about 1910.78/2092 MiB (Lighting adapter), a different
scope from DXGI usage. Cached Release package CPU validation/decode took 1.704 s and
GPU/window preparation 1.451 s including shader/IBL work. No realtime algorithm rewrite
was justified by these measurements; existing old-shading/composition caches were active.

Lower analysis is a quality option, not an equal-quality guarantee: cross-resolution normals
differed by mean 3.56° / p95 12.89° and fitted source confidence fell from .148 to .00363.
These are sensitivity measurements, not ground-truth accuracy. Current native identity was
0 LSB, same-device Recipe replay float-exact, WARP replay max error 5.96e-8. Unsupported
regions and real-photo residuals remain; these numbers do not certify arbitrary inputs.

## Quality / identity evidence matrix

| Invariant | Current evidence entry | Meaning / boundary |
|---|---|---|
| no-op, zero strength, confidence zero | ImageRatioTests / RelightingReliabilityTests; native/zero export | RGB8 ≤1 LSB; identity alone does not prove recovered lighting |
| source hash, pixel alignment, high frequencies | RecipeTests; native export / AnalysisGpuTests | fixed source remains independent of mesh holes and analysis size |
| light direction, normal sign | lighting Python tests / ReferenceTests / optimization GPU check | independent synthetic ground truth, travel direction uses `-direction` |
| specular, shadow and emission isolation | ImageRatioTests / SourceGeometryTests; specular/shadow/emission cases | conservative supported geometry; unknown stays protected |
| modes/input/descriptor ownership | ImageWorkspaceTests / WindowInputTests / cycle/views/reload | fixed UI slots and fence retirement, not unlimited reload proof |
| old package, bad load, cancel | ScenePackageTests / RecipeTests / transaction/missing/cancel | old scene and observation survive failure |
| replay/export | native/edit/reopen/ui/warp/release | same hardware exact replay; WARP float tolerance 2e-5 |

After adding an Optional stage, rerun the affected row, complete build/CTest/Python regressions,
and this finite-window demo in a new directory. Keep raw failures; only final repeated evidence
can mark a previously failed check passed.

## Provenance and limits

See [model and demo provenance](ModelProvenance.md). Public photo availability is not a license.
No weights, downloaded photos, cache or generated evidence belong in Git. There is no new
distribution license grant for the application itself in this stage.

Single-view geometry has unseen/back-facing/off-screen gaps. SAM masks are not semantic truth.
Intrinsic decomposition and one directional+ambient fit are ambiguous; confidence is heuristic.
Specular is bounded residual response, not full reflection; glass/refraction/GI/hidden geometry
are unsupported. Dark/saturated pixels cannot create lost texture. New output may clip LDR.
Native export rejects above 8192 per edge or 8 MiPixels and explicit readback budget; it does
not silently shrink to analysis resolution. Very large paintings can be viewed as Original
but require an explicitly prepared smaller input to fit current full relighting/export budgets.
No general Undo, scene database, arbitrary 3D edit save, volumetric/3D fog, reference registration or Agent. Optional 33 adds only the bounded image atmosphere described below.

Official measurement references (checked 2026-10-06):
[DX12 timestamps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing),
[DXGI memory definition](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info).
## Optional 33 regression note

Additional image atmosphere is independently default-OFF and uses fixed source observations. Rerun this Core matrix plus [ImageFog verification](ImageFog.md) after enabling it. The profiling scenarios should distinguish fog OFF, unchanged fog, density editing and simultaneous lighting editing. New recipes use v2 and read previous v1 files; ScenePackage v1 stays unchanged. Historical Core measurements above do not measure the optional pass.
