# Paired cast-shadow transport (stage 27)

This stage uses saved observations; it installs no models, changes no ScenePackage v1 or sidecar schema, and never fills hidden geometry. `SourceGeometry` builds a fixed LH camera-space shell from AnalysisMaps position/normal/validity/region. It rejects cross-region triangles, relative Z steps > 0.05, edges longer than 0.08 × nearer Z, and absolute coordinates above 1e6 source units. The limit is 1,048,576 analysis pixels; the canonical source is not resized. Excluded sky/emission/glass-like region names from the stage-26 hashed evidence are also excluded from the shell.

`ImageCastShadowPass` reuses `GpuScene`, `ShadowPass` and `Shadow.hlsl`. It owns two independent 2048² D32 shadow maps and two RGBA32F visibility targets. Source and target directions have independent cache keys. Editing Scene transforms/materials/visibility or the free camera cannot change this shell. PCF radius 0–2, depth bias and normal bias are image-session parameters; they do not modify 3D shadow settings. Normal bias uses the saved position map's units; depth bias uses light clip depth [0,1].

The image correction transports **diffuse directional visibility only**. Let I be decoded canonical RGB, R the positive intrinsic non-diffuse residual, B/D the saved ambient/direct coefficients, c = max(N·−direction,0). The diffuse source anchor is max(I−max(R,0),0); its relative reflectance is anchored with B_source + D_source c_source V_observed and an epsilon floor. The supported correction is reflectance × D_target c_target × visibility-change, bounded to [-min(2I,diffuse-anchor), 2I] before confidence/strength. This is added to the stage-26 output, not multiplied as another shadow ratio. Ambient and positive R receive no additional shadow multiplier. The previous specular contribution is retained; reconstructing all specular visibility is outside this baseline.

Removing darkness requires stage-26 old-photo support and an old geometric blocker. New darkness requires a new geometric blocker, old geometric visibility ≥ .9 and old observed visibility ≥ .85; unexplained baked darkness is not stacked with another mask. Dark/saturated inputs, invalid/back-facing receivers, low material/geometry/light support, high uncertainty and protected regions reduce or disable changes. Default shadow strength is .65; the analytic smoke uses 1. Identical lights, disabled cast shadows, zero strength/confidence, no target direct light, invalid source calibration or absent evidence preserve the previous output. This is conservative transport, not an exact inverse renderer or texture recovery from black shadows.

## Run and inspect

From the project root:

```powershell
conda run -n image-scene-renderer python tools/reconstruction/cast_shadow_examples.py --output generated/my-cast-fixtures
./build/Debug/ImageSceneRenderer.exe --package generated/my-cast-fixtures/plane --work-mode image --image-view cast-final --ui
```

The generator refuses an existing output directory. In Inspector, edit **Target lighting / relighting → Travel direction**. Use **Paired cast shadows**, **Cast shadow strength**, **Shadow confidence scale**, **Image shadow PCF radius**, **Image shadow depth bias**, **Image shadow normal bias**. **Reset target to source** restores the source lighting. Source calibration must be explicitly applied; doing so invalidates old photographic evidence until exported/refit/reanalyzed.

New appended `--image-view` keys (existing values retain their meaning): `cast-old-map`, `cast-new-map`, `cast-old-visibility`, `cast-new-visibility`, `cast-old-estimate`, `cast-confidence`, `cast-change`, `cast-difference`, `cast-final`, `cast-baseline`. Depth maps are square; image-domain views retain source aspect. Gray=.0 change for `cast-change`; difference is displayed ×4. Purple signals unavailable/stale/unsupported diagnostics. `relighted` also displays the final composition with this correction; `cast-baseline` preserves the previous composition for comparison. Source RGB and the 3D Final path remain separate.

```powershell
./tools/Build.ps1 -VisualStudioPath D:/VisualStudio
./tools/Validate-CastShadow.ps1
conda run -n image-scene-renderer python tools/reconstruction/verify_cast_shadow.py
```

The verification matrix uses `generated/prompt27-final-fixtures`, generated using that output argument, and the existing saved `scene26-final-indoor`/`scene24-final-indoor`. The real-package checks require those prior local artifacts; they are not downloaded or silently synthesized. Float readbacks are in `generated/prompt27-*-shading`; `readback.json` records both shadow matrices/update counts, coverage/reason, parameters, nonfinite counts and each capture's actual resource state. `baseline26.bin` is retained beside `result.bin`.

If another window locks the default executable, the same build and smoke scripts accept `-BuildDirectory generated/build-prompt27`. Launch its `Debug/ImageSceneRenderer.exe` explicitly; a separate build does not update the locked default executable. Readback tolerance is 2e-5, analytic shadow IoU ≥ .85 with no false positives beyond two analysis pixels, and supported restoration RGB RMSE < .01. Unknown/disabled cases are checked byte-for-byte. These tolerances do not imply true-photo ground truth.

Costs: the paired depth maps use 32 MiB, visibility uses 32 bytes/analysis pixel, and an extra final target uses 16 bytes/source pixel (256 MiB maximum, in addition to earlier targets). Source shell and upload buffers have additional costs. Uploads are released after their fence; whole prepared scenes/descriptors are retired with the existing graphics fence. No viewport resize reallocates or downsamples the source anchor.
