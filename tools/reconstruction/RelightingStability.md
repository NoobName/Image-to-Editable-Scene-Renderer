# Stability contract (stage22)

The final path remains `linear(original RGB8) * ratio`. No model, intrinsic decomposition,
specular removal or cast-shadow synthesis is added. The six weights are **heuristics with
different provenance**, not probabilities. Flat tangent-normal confidence does not disable
the geometry normal. Full source resolution is retained.

`RelightingParameters` defaults: strength 1, epsilon .02 in the stage19 median-proxy relative
shading gauge, ratio [.25,4], chroma limit 1.5, relative depth edge .1, normal cosine edge .85.
The **Conservative preset** keeps strength 1 and sets epsilon .03, ratio [.5,2], chroma 1.25,
depth .08, normal .9. Uncheck **Use stability controls** for the stage21 comparison (manual
protection still applies). Reset target does not reset these controls; restart resets the session.

`RelightingReliability` builds two analysis-resolution RGBA32F textures, separate from PBR:

| Component | Rule / missing-data fallback |
|---|---|
| Geometry / normal | Synthetic 1; binary-validity provenance .8; other quality .25+.75q; missing .5; invalid 0 |
| Region / boundary | .5+.5 proposal score, missing .5; four-neighbor invalid, depth, normal or label transition caps .35 |
| Material / reflection | Neutral fallback .75; other quality .35+.65q; missing .5; multiply roughness and metallic protection ramps |
| Fit | exp(-luminance residual/.35) in fit support; missing/outside .5; explicit manual/test source 1 |
| Signal | smoothstep(.002,.04,YoriginalLinear) * (1-smoothstep(.92,.98,max(originalEncoded))) |
| Shadow risk | With intrinsic proxy support, clamp((Yproxy/(Yold+.02)-.2)/.6,.25,1); unknown .8 |

Reflection ramps are clamp((roughness-.08)/.32,0,1) and clamp((.85-metallic)/.6,0,1).
Source calibration invalidates old fit evidence: fit becomes .5, shadow risk .8 until a new
offline fit is loaded. The source/target calculated shading still recomputes normally.
These thresholds are fixed conservative policy, not learned calibration or shadow classification.

Total `c = validity * (1-protection) * min(all six weights)`; validity is a hard gate.
Let a/b be old/new calculated RGB shading and Y their Rec.709 luminance. Define
`l = log((Yb+epsilon)/(Ya+epsilon))`, `h = log((b+epsilon)/(a+epsilon))-l`.
Luminance mode sets h=0. Brightening support is the minimum of smoothstep(.01,.12,Ya)
and smoothstep(.002,.04,Yoriginal). Interpolate the maximum brightening from
min(1.15,maxRatio) to maxRatio using this support. Clamp l to [log(minRatio),log(brightening)],
h to +/-log(chromaLimit), then their sum to [log(minRatio),log(maxRatio)].
`effectiveLog = strength * c * limitedLog`; `ratio = exp(effectiveLog)`.
Invalid, exact old=new, strength=0 or c=0 explicitly returns ratio=1.

Analysis resampling uses bilinear values **only when all four neighbors** are valid,
share the center's integer region label, have normal dot >= the configured threshold and
relative depth difference <= threshold. Otherwise it uses the center's nearest value.
It never uses original RGB edges to smooth the original. Native-size mapping is nearest;
coordinates within 1e-4 analysis pixels of an exact center are snapped to eliminate float noise.

Import an opaque grayscale PNG (<=2048 each edge, <=16 MiB) with
`--protection-mask path.png`. Black allows edits, white forces original, gray proportionally
reduces edits. Values are data, no sRGB conversion. It is nearest-mapped in normalized source
coordinates, fingerprinted and associated with sourceId; reloading a different source drops it.
This is session input, not a paint editor or package mutation.

Debug keys: geometry-weight, boundary-weight, material-weight, fit-weight, signal-weight,
shadow-risk-weight, relighting-confidence, raw-log-ratio, effective-log-ratio, clamp-mask,
protection. Weights display directly in [0,1]. Log displays are `.5 + logRatio/(4*ln(2))`.
Raw/effective zero is gray; clamp mask is white where limiting acted. These previews are not
numeric evidence: `*-shading/readback.json` and float binary captures are.

GPU: 11 SRV tables + 24 root constants = 35 DWORDs. Quality/mask uploads use existing
NumericTexture and PreparedScene fence lifetime; analysis resources are viewed, not duplicated.
QualityA/B readbacks explicitly transition PS_RESOURCE -> COPY_SOURCE -> PS_RESOURCE.

```powershell
conda run -n image-scene-renderer python tools/reconstruction/stability_examples.py generated/my-stability
./build/Debug/ImageSceneRenderer.exe --package generated/my-stability --work-mode image --image-view relighted --protection-mask generated/my-stability.protect.png
./tools/Validate-Stability.ps1
conda run -n image-scene-renderer python tools/reconstruction/verify_stability.py
```

Fixture generation writes a new directory only. The verification script targets the fixed
`prompt22-*` validation evidence. It checks CPU/GPU formulas, hard protection, identity,
trusted response, repeated trajectory byte equality and unchanged 3D Final.
