# Independent intrinsic analysis (stage23)

`IntrinsicBackend` is separate from MaterialEstimationBackend and LightingEstimationBackend.
It predicts linear albedo A, diffuse shading S, optional non-diffuse residual R, A/S/R uncertainty
and validity. The native decomposition is `linear(input RGB) ~= A*S + R`.
It does **not** replace material albedo or change the default stage22 relighting formula.

Backends: `marigold-lighting` (single pinned candidate), `proxy` (existing albedo and I/A;
no residual estimate; uncertainty unavailable), `saved` (validated existing float evidence).
Proxy's small recomposition error is an algebraic property, not proof of a recovered intrinsic.

```powershell
conda run -n image-scene-renderer python tools/reconstruction/fetch_intrinsic.py
conda run -n image-scene-renderer python tools/reconstruction/estimate_intrinsic.py generated/scene19-final --output generated/my-intrinsic --offline --resolution 512 --steps 4 --ensemble 3 --seed 23
./build/Debug/ImageSceneRenderer.exe --package generated/my-intrinsic --work-mode image --image-view intrinsic-shading
```

Use `--backend proxy` without torch inference or `--backend saved` to republish saved observations.
Output must be a new directory outside the input. No source, geometry, material or lighting bytes
are modified. Optional `intrinsic/intrinsic.json` version1 has its own strict schema, fingerprints,
native gauge, input mapping, provenance and recomposition statistics. It uses bounded DX10 DDS
and NPZ float data; diagnostic PNG is never a numerical input. Sidecar damage rejects the
candidate package, while a missing sidecar keeps the old 3D/image baseline available.

Desktop: **Estimate saved image intrinsics** in the source lighting inspector runs an independent
Python process, stages `intrinsic / export` in existing progress protocol v2. Backend is chosen in
File > Reconstruction settings > **Intrinsic (offline only)**. Cancel and failures retain the active
scene; loading publishes the complete CPU/GPU session. `--estimate-intrinsic <package>` and
`--intrinsic-backend proxy|saved|marigold-lighting` drive the same workflow. Defaults use cached
models only. Legacy progress v1 and full reconstruction stage lists are unchanged.

## Candidate and output semantics

Official [Marigold Lighting model card](https://huggingface.co/prs-eth/marigold-iid-lighting-v1-1)
and pinned [target_properties](https://huggingface.co/prs-eth/marigold-iid-lighting-v1-1/blob/08c3930bb641abf786ba44ce92547507ebefbc16/model_index.json):
all three outputs are linear. Appearance's albedo sRGB decode must not be copied here. Lighting
shading/residual are up-to-scale; we preserve the checkpoint's native gauge (all gains1, exposure0)
and report error without per-pixel correction. Residual is not a specular mask.

Pinned revision `08c3930bb641abf786ba44ce92547507ebefbc16`; safetensors SHA256:

| File | SHA256 |
|---|---|
| text_encoder/model.fp16.safetensors | bc1827c465450322616f06dea41596eac7d493f4e95904dcb51f0fc745c4e13f |
| unet/diffusion_pytorch_model.fp16.safetensors | 8c2e8da73793181f87c9c7a752a939d6f6834104e9854bf1c53c6b37de3594d9 |
| vae/diffusion_pytorch_model.fp16.safetensors | 3e4c08995484ee61270175e9e7a072b66a6e4eeb5f0c266667fe1f45b90daf9a |

Weights use RAIL++-M, code Apache-2.0; retain the official card/license conditions. No repository
Python code is downloaded/executed. Existing locks remain diffusers0.35.2, transformers4.55.4,
accelerate1.10.1, safetensors0.6.2, torch2.8.0+cu129. No upgrade/isolated environment was needed.
Inference freezes modules, batch1, fp16 on CUDA, model CPU offload and VAE slicing; release
hooks, local references and temporary cycles before the next model. CPU uses float32.

The installed Diffusers0.35.2 decoder bounds native predictions to [0,1]; this adapter declares
**nonnegative non-diffuse residual**. The general contract also accepts signed [-64,64] residuals
from other declared adapters/tests; it never globally clips negatives into specular. Albedo is
[0,1], shading [0,64], uncertainty [0,64], validity binary uint32. Vector DDS padding W is0.
Uncertainty RGB means maximum RGB ensemble standard deviation for A/S/R separately; unavailable
uncertainty is1 with explicit metadata, never an invented certainty. Geometry validity is separate.

Debug keys: `intrinsic-albedo`, `intrinsic-shading`, `intrinsic-residual`, `intrinsic-uncertainty`,
`intrinsic-error`, `intrinsic-validity`. Existing `albedo` is now labeled Material Albedo to distinguish
the observations. A/S display linear→sRGB once, residual uses gray .5 + .5R, error is per-pixel RGB
RMS ×4 for visibility, uncertainty is direct data. All clips affect previews only. Missing residual
shows unavailable stripes. GPU captures preserve exact bytes and explicit resource states.

Alternative research only: [Colorful Intrinsic official repository](https://github.com/compphoto/Intrinsic)
declares academic-use-only conditions and separates negative/positive residual interpretation.
It is not installed or used here; commercial suitability is not inferred from public availability.

## Validation

```powershell
./tools/Build.ps1 -VisualStudioPath D:\VisualStudio
./tools/Validate-Intrinsic.ps1
conda run -n image-scene-renderer python tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
conda run -n image-scene-renderer python tools/reconstruction/verify_intrinsic.py
```

Real candidate comparison covers the saved indoor photo, a pinned MoGe traffic example and a
Met public-domain painting. Their inputs/URLs/hashes and actual device/timing/memory/metrics are
local generated evidence. These three samples are not a benchmark of general quality, and low
recomposition error alone cannot disambiguate A/S/R or prove a physical lighting solution.
