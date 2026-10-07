# Additional image atmosphere (Optional 33)

This is depth-guided **additional relative haze** on the immutable image observation. It is not dehazing, recovered atmospheric scattering, volumetric light, or a 3D fog implementation. `LookParameters::fogDensity` remains reserved and inactive for 3D.

Build with `tools/Build.ps1`. In Image Relighting mode, open **Additional image atmosphere**, enable **Add image atmosphere**, adjust **Additional density** and **Airlight (linear RGB)**. **Reset additional atmosphere** disables the effect. Use **Allow relative / synthetic scale** only to acknowledge arbitrary reconstruction units. Geometry, source pixels, source camera and source lighting are not edited.

```powershell
./tools/Build.ps1 -BuildDirectory generated/build-fog
python tools/reconstruction/fog_examples.py generated/fog-inputs
./generated/build-fog/Debug/ImageSceneRenderer.exe --package generated/fog-inputs/edge --work-mode image --image-fog-density 0.3 --ui
./tools/Validate-ImageFog.ps1 -BuildDirectory generated/build-fog -Inputs generated/fog-inputs -Output generated/fog-validation -Python python
```

Use a new output directory; the scripts retain previous evidence. CLI density must be finite in `[0,1000]`; the everyday UI slider is `[0,10]`. `--image-fog-relative` enables arbitrary relative/synthetic units. Commands and UI share `RelightingSession::fog` and `ImageFogParameters::Valid`.

## Contract and composition

Source additional density is fixed at zero: baked source haze is unknown, not estimated as zero. Target density adds an effect to the current image. Let `B` be the existing linear display-referred result after diffuse/specular/cast-shadow editing:

```text
P = ( (u-cx)*Z/fx, -(v-cy)*Z/fy, Z )    // LH camera, normalized pixel-center UV
d = length(P)                          // meters, relative-units or synthetic-units
t = exp(-min(density*d,80))
c = geometryConfidence * (1-protection), gated by valid geometry, known scale and non-sky
T = 1 - c*(1-t)
F = T*B + (1-T)*airlightLinear
display = sRGBEncode(F * 2^displayExposure)
```

Validated point maps are preferred. Missing position uses camera-Z and the retained analysis intrinsics, never the edited 3D camera. No distance normalization is silently introduced: changing geometry scale changes the density needed for the same atmosphere. Relative/synthetic data is disabled until explicit acknowledgment. Unknown scale or absent observations is unavailable.

`ImageFogData` records distance/confidence/camera-Z/hard-validity as RGBA32_FLOAT at analysis resolution. Source resolution is retained. Sampling uses the source pixel center; four analysis samples may blend only when all are supported, have the nearest sample's stable label and lie within a 5% camera-Z difference. Otherwise the nearest supported sample is used; invalid nearest samples are protected. Declared sky labels are excluded. Undetected sky and incorrect model depths are not magically identified. Region/imported protection weights also limit haze, independently of the lighting-fit confidence: an unidentifiable light direction does not make a known plane distance unknown.

Fog follows cast shadows and precedes display exposure exactly once. It intentionally attenuates the full visible image (including ambient/emissive appearance), as path transmission does; it is not a second direct-light shadow mask. Airlight is an explicit linear RGB color, not current environment intensity or a second exposure term. Numeric debug views bypass Look, exposure and creative grading.

## Ownership, budget and persistence

`ImageFogPass` is owned by `ImageRelightingComposite` within the existing prepared scene. It borrows the preceding result and protection texture with the same lifetime. It owns one native RGBA32_FLOAT target, a distance texture and labels, with fixed descriptors. The additional target is at most 128 MiB (8M source pixels); beyond that the effect is unavailable without downscaling the anchor. This allocation exists even when disabled; disabling chooses the previous result directly and submits no fog draw. Unchanged parameters/input revisions reuse the cache. Fog-only edits do not recompute Old/New shading.

Draw transitions SRV→RT→SRV; explicit capture transitions SRV→COPY_SOURCE→SRV and waits before mapping. Ordinary interactive frames do not read back images. Existing fence retirement also owns the fog resources through reload/cancel/failure.

New saves use **RelightingRecipe v2**, renderer `image-relighting-33-v2`, parameter contract `bounded-response-fog-v2`. The strict shared schema includes versioned `state.imageFog` with `sourceAdditionalDensity:0`, `enabled`, `density`, `airlightLinear`, `allowRelativeScale`. New readers still accept v1 and reset additional fog to OFF rather than inheriting memory. Old executables correctly reject v2 instead of silently losing the effect. ScenePackage v1 and its canonical anchor remain unchanged; recipes stay outside the package with existing atomic publication.

## Debug and verification

Existing Relighted displays the final atmosphere result. New image-view keys are `fog-distance`, `fog-transmittance`, `fog-confidence`, `fog-airlight` (UI group **Additional image atmosphere**). Distance is divided by the reported supported maximum, purple means invalid. Effective transmission is 1 in protected/disabled regions; confidence is zero there. Airlight preview is raw linear RGB contribution. No fictional sky fill is generated.

Capture adds `fogDistance.bin`, `preFog.bin`, and fog metadata. Native export adds `fog-distance.dds` and `pre-fog.dds`; `result.dds` remains the actual linear display-referred GPU result, not HDR radiance. `verify_image_fog.py` independently checks distance from point maps, guarded sampling, exponential transmission, final GPU output, exact protection, recipe replay, WARP agreement, finite values and restored resource states. `fog_reference.py` is a verification implementation, not a Python runtime dependency.

After this optional feature, rerun the affected [Core Demo](CoreDemo.md) identity/performance/recipe/dual-mode checks. See local ignored `docs/validation/33-results.md` for actual configurations and results. This feature does not require new models or Optional 34–35.
