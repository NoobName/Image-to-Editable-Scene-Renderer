# Intrinsic PBR material estimation

Stage 13 adds `MaterialEstimationBackend`, implemented by an optional Marigold IID Appearance adapter and a model-free neutral fallback. No training is performed. Geometry, segmentation and material backends remain independent; C++ only consumes files.

## Model choice and installation

The [official Marigold IID Appearance model](https://huggingface.co/prs-eth/marigold-iid-appearance-v1-1) predicts intrinsic albedo, roughness and metallicity. Its albedo is sRGB; roughness/metallicity are linear data. The [Diffusers integration](https://huggingface.co/docs/diffusers/en/api/pipelines/marigold) provides the maintained inference API. This is an inverse-rendering estimate, not a measured reflectance map: lighting leakage, smoothing and material ambiguity remain possible.

Also evaluated: [Colorful Diffuse Intrinsics](https://github.com/compphoto/Intrinsic) separates reflectance, shading and residual, with an academic-use implementation; [SuperMat](https://github.com/hyj542682306/SuperMat) estimates PBR maps and recommends isolated RGBA foreground objects. Marigold Appearance was selected for the current whole-scene photographs and its direct albedo/roughness/metallic outputs. No candidate was selected merely for converting RGB brightness into roughness or normals.

```powershell
conda activate image-scene-renderer
.\tools\reconstruction\setup-material.ps1
```

The script adds pinned Diffusers 0.35.2, Transformers 4.55.4, Accelerate 1.10.1 and Safetensors 0.6.2 to the dedicated torch 2.8.0 environment. It downloads only official fp16 safetensors and required configs (~2.58 GB) to ignored `.cache/huggingface`. The existing torch/torchvision versions stay unchanged. `environment.yml` includes the new dependencies for fresh environments; model fetching remains explicit.

Checkpoint revision: `e7280a0a0fc5a0df0b36050882b3d8b77da22fd9`. All three weight SHA256 hashes are pinned in `pipeline/adapters/marigold_config.py` and verified before use. Inference uses installed Diffusers code, without remote Python execution or pickled model weights. Code and model licenses differ: the model card specifies CreativeML Open RAIL++-M weights; the original project code is Apache-2.0. Source references and limitations are recorded in each output package.

Tested locally with Python 3.13.7, torch 2.8.0+cu129 and RTX 5070 Laptop 8 GB on native Windows. CUDA uses fp16, sequential batch size 1, VAE slicing and CPU model offload. Geometry and segmentation models release GPU resources between stages. CPU is supported by the adapter but real material inference was validated on CUDA.

## Run

```powershell
# Existing stage-11/12 predictions: run only material estimation, retain region IDs and masks
python tools/reconstruction/estimate_material.py generated/scene12 --output generated/my-material13 --device cuda --offline

# Full image -> geometry -> segmentation -> intrinsic material -> ScenePackage
python tools/reconstruction/reconstruct.py input.jpg --output generated/my-scene13 --geometry-backend moge --segmentation-backend sam2 --material-backend marigold --device cuda --max-size 512 --offline

.\build\Debug\ImageSceneRenderer.exe --package generated/my-material13 --render-mode estimated-albedo
```

`estimate_material.py` defaults to `marigold`. `reconstruct.py` and the pipeline API default to **neutral material** to preserve a lightweight path: neutral linear albedo 0.5, roughness 0.8, metallic 0, flat tangent normal, confidence 0. A neutral fallback does not pretend the illuminated photograph is albedo. Legacy photo-placeholder fixtures and old packages remain readable. For a new neutral package, choose **Original Image** to inspect the photograph.

Existing local demos: `generated/scene13` (SAM-segmented indoor scene with new intrinsic material) and `generated/scene13-inference` (all three real models run from the input photo). Outputs must be new/empty; old packages are never overwritten. Material-only rebuilding uses saved mesh filters and checks original vertex/triangle counts. Stored camera, lights, environment/HDRI, Look, transforms and visibility are retained; unsaved Inspector edits are not available to the file pipeline. Use `remesh.py` or `segment.py` on a stage-13 package to retain saved material maps without rerunning the material model.

| Option | Default | Meaning |
| --- | --- | --- |
| `--material-backend` | neutral / marigold, depending on CLI | Uniform fallback or actual intrinsic inference |
| `--material-resolution` | 512 | Internal inference longest edge, 128–1024; output resized to processed image |
| `--material-steps` | 4 | Denoising steps, 1–50 |
| `--material-ensemble` | 3 | Repeated predictions, 1–8; uncertainty available at 3+ |
| `--material-seed` | 13 | Random seed, uint32 |
| `--device` | auto | auto / cuda / cpu; requested unavailable CUDA fails clearly |
| `--offline` | false | Require cached files; no network download |

The fixed seed improves repeatability; it does not guarantee identical bits across hardware/library versions. Lower ensemble size reduces work; it is not equivalent to reliable uncertainty. No failed inference silently becomes neutral or copies the photo.

## Uniform contract

`MaterialEstimate` contains CPU-owned float32 arrays at input image resolution:

| Field | Shape / convention |
| --- | --- |
| albedo | H×W×3, **linear** reflectance in [0,1] |
| roughness | H×W, perceptual roughness [0,1], not GGX alpha squared |
| metallic | H×W, [0,1] |
| normal | H×W×3, signed unit **tangent-space** XYZ, +Z along the geometric normal |
| confidence | H×W, [0,1], precise semantics in metadata |

The adapter explicitly checks the model's `target_properties`. It decodes sRGB albedo to linear and reads the material tensor's R=roughness, G=metallicity, B=unused. Export subsequently packs the renderer's glTF convention G=roughness, B=metallic. A model visualization/colormap is never used as numeric data.

Appearance does **not** predict detail normals. The normal output is explicitly `(0,0,1)`, `normal_source=flat-tangent-fallback`, normal confidence 0; normal strength defaults to 0 to avoid 8-bit neutral-normal quantization tilt. Existing MoGe mesh normals remain active. Camera/world geometric normal maps must not be bound as tangent normal maps. A future adapter may provide estimated tangent normals without changing the exporter or renderer.

For 3+ ensemble members, predictions use the mean and uncertainty uses standard deviation. Confidence is `1 - max(std)` over albedo RGB, roughness and metallicity, excluding the unused channel. It expresses consistency, **not calibrated accuracy**; it does not certify the fallback normal. With no uncertainty, confidence is 0 with explicit metadata. All arrays are checked for finite values, dimensions, range and normal normalization.

## Files, color space and material binding

```text
textures/
  original_image.png              processed, oriented sRGB source photograph
  base_color.png                  compatibility copy of source; NOT the estimated material
  object_albedo.png               sRGB encoding of linear estimated albedo
  object_normal.png               linear data, N*0.5+0.5
  object_roughness.png            linear grayscale, R channel
  object_metallic.png             linear grayscale, R channel
  object_confidence.png           linear grayscale
  object_metallic_roughness.png   glTF packed map: R=1, G=roughness, B=metallic
debug/
  material.npz                   lossless float32 uniform output arrays
  material_estimation.json        revisions, parameters, confidence semantics, fallbacks
  reconstruction.json            combined geometry/segmentation/material report
```

Each segmented object gets its own material instance referencing this shared full-image atlas; UVs and object IDs stay unchanged. Files are shared intentionally instead of copying the same pixels once per object. The manifest's material adds `originalImage`, `confidence`, `albedoSource`, `normalSource`; Python and C++ validate the same schema. Confidence is preserved as an analysis asset, not multiplied into PBR lighting. Albedo/original use sRGB SRVs, while normal and MR use linear SRVs.

`roughnessFactor=metallicFactor=1` preserves the estimated texture values. Inspector sliders multiply these maps; a factor of 1 does not mean every pixel has roughness/metallicity 1. Full `meshes/scene_mesh.glb` embeds albedo, packed MR and normal PNGs for standalone loading. Per-region GLBs reference the shared atlas; the source photo remains a ScenePackage debug binding.

## Viewer modes

| UI / CLI | Meaning |
| --- | --- |
| Original Image / `original` | Source photograph projected on the same geometry, independent of material factors |
| Estimated Albedo / `estimated-albedo` | Raw albedo texture, sRGB display, no material color factor |
| Estimated Normal / `estimated-normal` | Raw encoded tangent normal map; flat purple for the current fallback |
| Estimated Roughness / `estimated-roughness` | Raw roughness texture, linear grayscale, no roughness factor |
| Existing Albedo / Roughness / Metallic | Material texture multiplied by current editable factors |
| Existing Normal | World-space normal after material normal mapping |
| Final | Estimated PBR maps + current lighting/shadows + HDR/post processing |

New views bypass lighting, exposure, tone mapping, grading and bloom. Original/Estimated Albedo receive exactly one sRGB display encoding. Data maps stay raw. All views share the current camera and mesh, so holes/boundaries remain visible; Original Image is not a separate full-screen 2D viewer. Old packages lacking an original binding show magenta in that mode, with a UI tooltip explaining it. Inspector shows the albedo and normal provenance.

Final will not reproduce the input photograph merely because albedo is estimated: current lights, geometry and shadows are still approximate, and the original environment has not been recovered. Use raw views to diagnose each component separately.

## Validation

```powershell
python tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
.\tools\Validate-Material.ps1
```

The 58 model-free regression tests include 11 material cases. Actual CUDA inference is validated separately, both on an existing segmented package and through all three models. The window script expects the local stage-12/13 demo packages, creates an analytic fixture in a new directory, and checks exact known GPU colors/channels, raw-view independence from factors/look edits, finite HDR, old-scene compatibility, IBL/Shadow bindings, Release and WARP. Chinese learning notes, interview Q&A and detailed measurements remain under ignored `docs/`.
