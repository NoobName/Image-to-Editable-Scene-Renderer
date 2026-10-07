# Original-image shading ratio (stage 21)

Image Relighting defaults to `relighted`; explicit `--image-view original` remains available, and old 3D `--render-mode` integers are unchanged. The immutable canonical/legacy source RGB is the appearance source. Estimated albedo does not enter composition.

At each source pixel center, nearest analysis coordinates are `floor((p+0.5)*analysisSize/sourceSize)`. This baseline never interpolates across invalid support; later stability work can refine sampling without touching source RGB. If either shading alpha is zero, ratio=1.

Let `a=S_old`, `b=S_new`, `e=0.02` in the recorded median-normalized relative shading gauge, and `Y(v)=dot(v,(.2126,.7152,.0722))`. The symmetric base luminance ratio is `q=(Y(b)+e)/(Y(a)+e)`.

- Luminance mode: raw ratio=`q` in all channels, preserving source chromaticity (before output clipping). Changing light color still changes its luminance contribution.
- Bounded color mode: raw ratio=`q*clamp(((b+e)/(a+e))/q,0.5,2)` per channel. Target color affects chromaticity, subject to the cap.
- Effective ratio=`exp(strength*log(clamp(raw,0.25,4)))`; strength ∈[0,1]. Every clamp includes1; old=new, strength0, or invalid support returns1.
- Final linear RGB=`decode_sRGB(original_RGB8)*effective_ratio`. Every edit reads the immutable source, never the previous result. Display encodes once, with neutral exposure0, no tone mapping/Look/Bloom. UNORM clipping is output gamut clipping, not a tone curve.

The source resource has a separate UNORM SRV in this pass, with explicit piecewise sRGB decode. This avoids hardware approximate sRGB float differences in numeric validation; `SourceImagePass` keeps its existing sRGB SRV. Both paths decode only once.

`ImageRelightingComposite` owns full-source RGBA32F Ratio and Final targets, independent from PBR materials. Two derived targets have an explicit512MiB/16M-pixel limit; exceeding it makes composition unavailable, preserving full Source display. A source/target/parameter change recomputes from source. Other frames reuse targets. No AI is run by dragging lights.

```powershell
.\build\Debug\ImageSceneRenderer.exe --package generated/scene19-final --work-mode image --image-view relighted
# Target lighting / relighting: edit light, strength, color response, Reset target to source
conda run -n image-scene-renderer python tools/reconstruction/ratio_examples.py generated/my-ratio-detail
.\tools\Validate-ImageRatio.ps1 -Fixture generated/my-ratio-detail
conda run -n image-scene-renderer python tools/reconstruction/verify_image_ratio.py
```

`*-shading/ratio.bin` and `result.bin` are full source size float32 RGBA; existing old/new buffers remain analysis size. JSON records parameters and actual states. GPU timestamps cover changed-frame shading+ratio+composition, excluding inference/upload/present/readback/display. Startup CPU wall time is logged separately. This baseline supports modest diffuse-dominant edits; old cast shadows, reflections and lost black/saturated details remain.
