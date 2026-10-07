# Image-domain calculated shading (stage 20)

`ImageRelightingRenderer` consumes immutable camera-space geometry normal/validity from `AnalysisTextures` plus the independent `LightingSession`. It does not consume Scene camera, edited meshes, PBR materials, shadows, environment maps or Scene lights.

Both RGBA32_FLOAT buffers use **unit-reflectance response in the relative fit gauge**:

`S = directColor * directIntensity * max(dot(normalize(n), -normalize(travel)), 0) + ambientColor * ambientIntensity`.

There is no additional `1/pi`: the fitted coefficients already absorb it and the unknown exposure scale. This is not irradiance in physical units, nor albedo-multiplied PBR Final. Alpha is geometry validity; invalid samples are RGBA zero. The full source image remains independent of validity.

The bounded image root layout has four SRVs and 20 DWORD constants (24 DWORD root cost); its independent heap has 16 SRV slots and two RTV slots. Evaluation binds null SRVs for output inputs to prevent read/write feedback. Targets are analysis-sized, single-mip, SRV→RT→SRV. Window resize only changes presentation. A new prepared scene owns new descriptors/targets; previous instances retire on the existing graphics fence.

Source parameters/revision refresh Old; target parameters refresh New. Unchanged frames reuse both. Source calibration may invalidate the *offline fitted evidence*, while the calculated Old response can update immediately. No neural inference or texture re-upload occurs during target edits.

```powershell
.\build\Debug\ImageSceneRenderer.exe --package generated/scene19-final --work-mode image --image-view calculated-new
.\tools\Validate-ImageShading.ps1
conda run -n image-scene-renderer python tools/reconstruction/verify_image_shading.py
```

Views: `calculated-old`, `calculated-new`, `shading-difference` (absolute RGB difference), `normal-light-dot` (negative=red), `shading-validity`. Shading previews divide by four then sRGB encode for display only. Source remains the default; this stage generates no ratio or relit RGB. Existing `--render-mode` values and 3D Final are unchanged.

`*-shading/readback.json` records actual source/target, update counts, size, scale, validity, nonfinite checks and capture states. `old.bin/new.bin` are tightly packed little-endian float32 RGBA. `shading_reference.py` independently evaluates float64 values. CPU waits for the capture fence before mapping the readback resource.
