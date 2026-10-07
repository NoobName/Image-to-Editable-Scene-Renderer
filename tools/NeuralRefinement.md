# Optional offline neural refinement (stage 34)

The Core renderer remains the default and usable fallback. An explicit **Recipe / Export →
Optional offline neural refinement → Run optional neural refinement** action snapshots the
current recipe and native physics export, then starts an independent Python process. Dragging
a light never starts inference. **Cancel refinement** terminates that job. The **Compare
Original / Physics / Refined** button opens a local offline HTML comparison; the Main Viewport
keeps physics. No candidate is automatically accepted as a source image or geometry estimate.

## Setup and offline operation

Use the project's CUDA Python environment configured in Reconstruction settings. Core32's
Marigold IID Lighting checkpoint must already be cached. This optional adapter reuses Torch,
Diffusers, Transformers, safetensors and OmegaConf; its additional dependencies are installed
into ignored `.vendor/refinement-deps`, without changing the Core environment. Run only after
reviewing the [model terms](ModelProvenance.md#optional-stage-34-research).

```powershell
conda activate image-scene-renderer
./tools/Fetch-Refinement.ps1 -Python (Get-Command python).Source -Weights -Dependencies
```

This explicit network preparation fetches pinned official source and 2.564 GB weights, verifies
Git blob hashes / SHA256, and keeps them ignored. It requires 6 GiB free disk before downloading.
The adapter itself uses local files only. Missing assets, CUDA, memory, or dependencies produce
a diagnostic and retain physics. Core startup never imports Torch or downloads this model.

Given a matching existing Core recipe and its export, use a **new** output directory:

```powershell
python tools/reconstruction/refine_image.py --recipe generated/core-demo/real-edit.json --physics-export generated/core-demo/real-edit --output generated/refinement-example --strength 0.2 --seed 34 --max-side 256
python tools/reconstruction/refine_image.py --replay generated/refinement-example/refinement.json --output generated/refinement-replayed
```

`--strength` defaults to **0**, copying physics bytes without model loading. Unchanged lighting
or identity physics pixels also bypass inference. `--protect-mask mask.png` is an additional
white=protect mask; imported Core masks and stable region protection are always respected.
`--cancel-file path` is checked before/after inference and publication. An active CUDA kernel
is not cooperatively preempted; the desktop workflow can terminate the whole owned process.
Use `--no-cache` to measure a fresh process inference rather than candidate replay.

## Actual guidance and limits

`NeuralRefinementBackend` receives immutable native canonical RGB8, physics-relighted
display-linear RGB, explicit source/target lights and global gain, camera-Z depth, LH camera
normals, old/new shading, geometry/native relighting confidence, validity and protection.
The initial real adapter is **PIXLRelight**. It derives target A/S/R (albedo, shading, residual)
from the **fixed physics RGB** with its bundled Marigold interface, then conditions on those
nine channels and the source RGB. Depth/normals/old-new maps are external validation and
acceptance guidance, **not** invented network input channels. Target lighting is conveyed by
the authenticated physics result; no free-form prompt or generative image API is used.

The tested budget is processing side 256, native 1500×1000, on RTX 5070 Laptop 8 GB. Original
and physics are resized together to patch-aligned dimensions and padded; predicted gain/bias
are unpadded and upsampled before native source modulation. Native limits are 8192/edge and
8 MiPixels; explicit side 512 is available but not validated in this stage. Free CUDA memory
must be at least 3 GiB (256) / 4 GiB (512); allocation failure still falls back. Sequential
intrinsic/network model ownership reduces simultaneous allocations. The measured CUDA tensor
peak excludes Renderer/DWM and is not total adapter-wide VRAM.

The model consumes display-referred sRGB RGB8. Linear blending after decode does **not** recover
HDR radiance or clipped data. Refinement weight is strength × (1−protection) × minimum of
geometry and Core relighting confidence × acceptance. Strong source edges, invalid geometry,
dark/saturated source pixels and excessive color/edge changes are conservatively rejected.
Protection preserves physics exactly; it cannot guarantee automatic OCR correctness, fix glass,
infer hidden geometry or prove the new illumination physically correct. Day-to-night has not
been validated. The indoor-trained model itself has limitations on strong shadows/highlights.

## Files, cache and reproducibility

`refinement.json` is an independent version-1 wrapper validated by
[`neural-refinement.schema.json`](../schemas/neural-refinement.schema.json). Core's recipe and
ScenePackage v1 remain unchanged. `physics-recipe.json` freezes the Core baseline/target,
response/shadow/specular settings and source identity; optional masks are copied with it.
Relative package/export paths remain portable when their directory relationship is preserved.
The wrapper records exact code/checkpoint/adapter revision, seed/parameters, input/map/mask
hashes, output hashes, provenance and content-drift metrics. A changed export state, source,
analysis, revision or hash rejects replay. Never regenerate a wrapper by hand to bless stale
physics pixels. A different machine/library stack may not be bit deterministic.

Outputs include `original.png`, `physics.png`, `raw-candidate.png`, `refined.png`,
`difference.png` (absolute display-linear difference ×4), `protected.png`, `accepted.png`
(effective weight), `rejected.png`, `guidance.png`, `guidance.npz` and `comparison.html`.
The report is fully local. No network is needed to inspect it. Existing result directories
are never overwritten; a sibling staging directory is atomically renamed only when complete.
Failure retains Core physics; process termination may leave a hidden staging directory, which
is not a published candidate. Cache entries live under ignored `.cache/refinement`, are keyed
by inputs/settings/revisions, and verify NPZ checksum, dimensions and finite values on replay.

## Validation and timing

```powershell
./tools/Build.ps1 -BuildDirectory generated/build-core
python tools/reconstruction/test_reconstruction.py
python tools/reconstruction/validate_refinement.py --recipe generated/core-demo/real-edit.json --physics generated/core-demo/real-edit --output generated/refinement-check
./tools/Validate-NeuralRefinement.ps1 -BuildDirectory generated/build-core -Python (Get-Command python).Source -Recipe generated/core-demo/real-edit.json -Output generated/refinement-window-check
./tools/Run-CoreDemo.ps1 -BuildDirectory generated/build-core -Python (Get-Command python).Source -Inputs generated/core-inputs -RealPackage generated/my-real-demo/run-0-512/final -Output generated/core-after-refinement
```

The Python real-model validator uses documented HouseIndoor sign ROIs, not an OCR detector.
Replace them for another input. It records protection/replay errors, inference versus cache,
fixed-seed rerun (declared tolerance ≤1 LSB), zero-strength byte identity and negative cases.
The window script checks actual terminal state as well as Debug Layer counts. A previous
600-frame cancellation fired after completion, so the final test cancels at frame 20.
Build, Core demo and GPU inference must run serially for meaningful GPU timing. UI explicit
export/readback is a one-time synchronous snapshot; subsequent Python inference runs off the
render thread. This stage adds no shader, descriptor or per-frame readback.

Model research, exact pinned revisions and separate rights are in [ModelProvenance](ModelProvenance.md).
Detailed Chinese teaching and measured evidence stay in ignored local `docs/`.
