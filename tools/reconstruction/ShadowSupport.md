# Fixed-source old cast-shadow support (stage 26)

This offline analysis does not change Source, image relighting, or the 3D Final renderer. No model is installed or trained. `ShadowEstimationBackend` is replaceable; the first backend is deterministic `depth-shell-support-v1`.

```powershell
conda run -n image-scene-renderer python tools/reconstruction/estimate_shadows.py generated/scene24-final-indoor --output generated/my-shadow-analysis
./build/Debug/ImageSceneRenderer.exe --package generated/my-shadow-analysis --work-mode image --image-view shadow-overlay --ui
```

The desktop **Analyze old-shadow support** button uses the existing independent Python process and atomic package loader. `--estimate-shadows PACKAGE` also starts that workflow. A valid accepted intrinsic-assisted source fit supplies the fixed shading normalization. Other calibrations require an explicit Python `--shading-scale K`; it is the scale for `S_observed / K`, not exposure. A proxy I/A decomposition cannot establish shadow evidence and yields zero automatic confidence.

Inputs are fixed source camera-Z depth, LH camera normal/position, normalized intrinsics, validity, region labels, material roughness/metallic, intrinsic albedo/shading/error/uncertainty, and source lighting. Saved light direction means travel; rays go toward `-direction`. The automatic candidate needs both direct-light attenuation and a different visible region intersecting the camera-depth shell. Missing or offscreen geometry, same-region concavities, weak light, unreliable intrinsic estimates and named sky/emission/reflection regions remain unknown. Names are heuristics, not a new semantic model. This is neither complete visibility nor ground truth, and cannot identify every old shadow.

`shadow/shadow.json` is an optional independent version-1 sidecar governed by `schemas/shadow.schema.json`. Eight single-mip R32 DDS maps preserve numeric values; `unknown` is R32_UINT, the others R32_FLOAT:

| Map | Meaning |
|---|---|
| candidate | supported automatic old-shadow amount [0,1] |
| visibility | observed direct-light fraction after fixed ambient subtraction [0,1]; meaningful only with support |
| geometrySupport | visible cross-region depth-shell intersection support |
| confidence | heuristic confidence in a positive old-shadow hypothesis |
| unknown | insufficient evidence, including apparently lit regions; not confirmed visibility |
| manualConfirm / manualProtect | independent imported human layers |
| effectiveCandidate | max(candidate, manualConfirm) × (1 − manualProtect) |

Dependencies fingerprint four source/analysis/intrinsic/lighting metadata files, the seven consumed geometry/region/MR DDS assets, and source region JSON. Intrinsic and lighting readers also check their own numeric hashes. Damaged or stale sidecars reject the new package. In-memory source calibration hides cached previews immediately. Current 3D edits and target light changes do not modify source evidence. Re-estimated light/intrinsic or re-exported analysis drops the old shadow sidecar in the **new output**; the input stays untouched.

Import opaque grayscale PNG masks (up to 2048 per edge, 4 M pixels, 16 MiB); normalized-source nearest mapping permits small masks. Re-run into a new output with `--confirm-mask MASK.png` and/or `--protect-mask MASK.png`. Omitted masks inherit existing human layers. Confirmation does not falsify automatic confidence or remove the unknown flag. After source calibration changes, masks must be reconsidered and explicitly imported again.

Debug keys: `shadow-candidate`, `shadow-visibility`, `shadow-geometry`, `shadow-confidence`, `shadow-unknown`, `shadow-manual-confirm`, `shadow-manual-protect`, `shadow-effective`, `shadow-overlay`. Purple stripes mean absent/stale data. Grayscale quantities are displayed directly; the red source overlay is a diagnostic, never a Final input. Labels/masks use nearest sampling.

Validation: `tools/Validate-Shadow.ps1`, `tools/reconstruction/verify_shadow.py`, and `tests/test_shadow.py` cover analytic shadow versus texture, missing/offscreen blockers, manual layers, stale dependencies, byte-exact GPU readback, resize/WARP/reload/cancel and unchanged Final. `shadow_examples.py` creates new deterministic fixtures and refuses to overwrite existing outputs.
