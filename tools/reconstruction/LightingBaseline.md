# Original lighting baseline (Prompt 19)

This stage fits and inspects the original light. It does **not** produce relighted RGB, apply target light to the photograph, or change the 3D Final renderer.

```powershell
conda run -n image-scene-renderer python tools/reconstruction/estimate_lighting.py generated/scene18 --output generated/my-lighting
.\build\Debug\ImageSceneRenderer.exe --package generated/my-lighting --work-mode image --image-view old-shading
```

The output must be a new/empty directory. Saved geometry/material/segmentation are reused without model inference. In Image Mode, **Fit saved observations** runs the same separate-process workflow and automatically imports the new package. `--fit-lighting <package>` is its command-line equivalent. Regular `reconstruct.py` now includes a lighting stage. `--lighting-backend robust-directional-ambient` is default; `--lighting-backend manual-test --light-direction X Y Z --direct-rgb R G B --ambient-rgb R G B` is explicit manual input. Coefficients are in the normalized proxy scale, not lux.

## Contract and scale

`pipeline/lighting_backend.py` defines `LightingEstimationBackend`, `LightingInput`, `LightingEstimate`. No model or DX12 types enter this interface. Fixed analysis RGB8 sRGB is decoded to linear, divided by fixed linear intrinsic albedo, then divided by its valid luminance median. Exposure is fixed at 0 EV, albedo gain at 1. The fitted model is `S = directRGB * max(dot(normal, -direction), 0) + ambientRGB`. Normals and stored **light travel** direction use LH camera coordinates (+Y up, +Z forward). The ambient term is spatially constant; it is not a recovered HDR environment or IBL.

The solver uses 512 spherical candidates, bounded deterministic sampling of 4096 pixels, Huber IRLS (7 iterations, delta 0.08), two-variable nonnegative active sets, and local angular refinement. It first fits luminance, then RGB at the fixed direction. Direction search is nonconvex and approximate. `beforeRMSE` is the weighted constant-ambient baseline; `afterRMSE` is the weighted luminance error of the final RGB fit, both on all supported pixels. These are not image reconstruction PSNR or ground-truth lighting accuracy.

Inputs rejected from fitting: invalid/non-unit normals; minimum albedo channel below 0.03; any RGB8 channel >=250; luminance below 0.003; metallic >0.3 or roughness <0.2; region names suggesting emitters/screens. The latter two are recorded heuristics. Albedo darkness and existing material confidence additionally weight accepted observations. Fewer than 64 samples, <5% support, deficient normal covariance, weak direct signal, ambiguous direction profile or out-of-range coefficients invalidate identifiability. Neutral/legacy albedo never claims intrinsic decomposition. Confidence is a heuristic diagnostic, not a calibrated probability. Legacy `MaterialPrediction` without estimated albedo keeps the old scene path and exports no lighting extension.

## Files and ownership

ScenePackage v1, `relighting/relighting.json` and `analysis/analysis.json` are unchanged. Optional `lighting/lighting.json` uses independent [strict schema](../../schemas/lighting.schema.json). It contains source identity/hash, analysis size, explicit scale/direction conventions, separate source/target light records, fit metrics/reasons/region residuals, eight input SHA-256 fingerprints and four output DDS hashes. Inputs: original processed RGB, geometry normal, albedo, validity, roughness, metallic, region IDs, material confidence. DDS follows the existing restricted DX10 loader: proxy/oldShading RGBA32_FLOAT (W=0), residual R32_FLOAT, fitMask R32_UINT. Same resolution as analysis. Source anchor is copied unchanged.

`lighting/fit_evidence.npz` retains weights and rejection bit flags (bit order: invalid-normal, black-albedo, saturated-rgb, dark-rgb, reflective-material, excluded-region). `diagnostic.png` and four PNG previews are display aids, never numeric input. Rejection counts may overlap. Region entries with zero supported pixels have no measured residual; their stored zero must be interpreted with `supported=0`.

`python tools/reconstruction/inspect_lighting.py <package>` validates the package and regenerates only diagnostic PNGs from saved DDS/NPZ evidence. The XY indicator points towards light, using screen delta `(-travelX, +travelY)` because camera Y is up and screen Y is down.

Both validators check paths, strict schema, dimensions, finite values, unit direction, support statistics, fingerprints, map formats/padding and invalid-pixel zeros. Missing sidecar is compatible; damaged sidecar fails loading. Fresh export uses staging + rename. Remesh/segment/material rebuilding creates fresh packages and drops stale lighting; rerun offline fitting. An analysis-only copy preserves lighting only if all fingerprints still validate, otherwise fails closed. Hand-editing an input and updating its hash is not recomputation.

Both readers also recompute the small Lambert prediction from the saved source parameters and fixed normal, and verify oldShading/residual within `1e-5 + 1e-5*abs(expected)`. Editing source parameters in JSON cannot silently retain an older shading cache. This CPU consistency check is separate from byte-exact GPU transport verification.

GPU `LightingPreview` owns four textures outside MaterialTextureCount and AnalysisTextures. It reuses NumericTexture, TextureReadback, FullscreenPass, PreparedScene and graphics-fence retirement. No relighting shader is introduced. Startup and successful background commits publish the source observation and matching lighting state together. Failure/cancellation keeps the previous complete document.

## Inspector and debug views

- Image View adds `shading-proxy`, `old-shading`, `lighting-residual`, `fit-mask`; old 3D `--render-mode` values remain unchanged. Source is still independent of geometry and lighting.
- Proxy and old shading use a fixed `/4` display scale followed by sRGB encoding. Residual shows absolute normalized luminance error (blue=0, red>=1). Fit mask is black/white. GPU excluded pixels are purple; unavailable/stale preview uses stripes. CPU montage uses black for rejected proxy/residual.
- Source calibration edits a draft. **Apply source calibration** normalizes a nonzero direction, marks source as manual, invalidates Old Shading/Residual, and leaves target unchanged. **Export calibrated source** reruns the offline manual backend into a new package to rebuild evidence. Until export, calibration is session-only. The displayed fit metrics remain explicitly historical after Apply.
- Target controls are inspection-only. They do not modify source or scene lights. **Copy source to target** is explicit. New packages initialize target from source. Reload publishes a new complete session.
- **Fit saved observations** imports a package reconstructed from saved files; it does not serialize unsaved 3D edits. Choose a new package only when ready to replace the current working document.

## Progress and validation

Progress v2 declares `stage_order`: full `geometry, segmentation, materials, lighting, export`, or offline `lighting, export`. Reader still accepts v1's four stages; unknown tables/states fail final validation. Snapshots remain atomic UTF-8 JSON with job identity and monotonic sequence.

```powershell
.\tools\Build.ps1 -VisualStudioPath D:\VisualStudio
conda run -n image-scene-renderer python tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
conda run -n image-scene-renderer python tools/reconstruction/lighting_examples.py generated/my-lighting-fixtures
.\tools\Validate-LightingBaseline.ps1 -Package generated/my-lighting -Fixtures generated/my-lighting-fixtures
```

The known-normal, multicolour Lambert fixture declares a direction tolerance of 3 degrees. The analytic normal field exercises the fitting contract; its plane mesh is only a transport fixture, not a claim of integrable reconstructed sphere geometry. Real photographs have no measured light-direction oracle. Record RMSE, support, ambiguity and failure regions; visual resemblance alone cannot establish recovery. No new model was installed for this stage.
