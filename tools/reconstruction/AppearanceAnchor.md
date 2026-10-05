# Appearance Anchor sidecar (Prompt 16)

This optional CPU-only contract preserves image evidence without changing ScenePackage v1 or Final rendering. An anchor is a reference asset, not albedo, recovered lighting, or a reconstructed HDR image.

## Assets and identity

| Asset | Meaning |
| --- | --- |
| `scene.json` | Existing v1 scene, including the editable viewing camera |
| `textures/original_image.png` | Existing processed RGB8 sRGB image at analysis resolution |
| `textures/source_anchor.png` | New canonical RGB8 sRGB image at the full EXIF-oriented size |
| `relighting/source/original.png` or `.jpg` | Byte-exact input file, including its original metadata/precision |
| `relighting/relighting.json` | Optional strict version-1 extension |
| `debug/source-analysis.png` / `.json` | Side-by-side preview, matching detail crops, dimensions and mapping report |

`sourceId` is `original-sha256:<raw file SHA-256>`. For legacy upgrades it is `legacy-processed-sha256:<processed PNG SHA-256>`. Every image record contains a lowercase 64-character SHA-256 of **encoded file bytes**, not raw pixels. Re-encoding identical pixels with a different PNG encoder may change its hash. Repack operations therefore copy existing assets and the sidecar byte-for-byte. Moving the package or changing analysis size for the same input file does not change its source identity. Hashes detect mismatches, not malicious provenance forgery or authenticity.

## Schema and validation

The shared strict schema is [relighting.schema.json](../../schemas/relighting.schema.json). The existing scene schema is unchanged. Supported fields:

- `version`, `sourceId`, `sourceKind` (`canonical` or `legacy-processed`).
- `sourceImage`, `analysisImage`: path, encoded-file hash, `[width,height]`, `colorSpace: srgb`, `encoding: rgb8`.
- `originalFile` for canonical sources: path, hash, stored raster size before EXIF orientation.
- `normalization`: pipeline identifier, original input mode, EXIF orientation and stored-to-canonical matrix, ICC policy, encoded-sRGB white compositing policy, explicit RGB8 precision limitation. Legacy uses `legacy-processed-unknown` / `rgb8-only`.
- `analysisMapping`: full-frame resize, filter, pixel convention and canonical-source-to-analysis matrix.
- `sourceCamera`: reconstructed or synthetic normalized/pixel intrinsics and reconstruction pose; otherwise `calibration-required` with a reason.
- `capabilities`: `fullResolutionAnchor`, `sourceCameraAvailable`, `requiresCalibration`, cross-checked against evidence.

Unknown fields (including speculative lighting/shading fields) and versions are rejected. The schema uses the existing validators' keyword subset: `$defs`, local `$ref`, `type`, `enum`, `const`, object properties/required/additionalProperties, array items/counts, string lengths, numeric bounds and `oneOf`. JSON parsing rejects duplicate keys and nonfinite numbers. Semantic validators additionally check relative path boundaries, lowercase SHA-256 content, actual decoded dimensions/format, coordinate matrices, intrinsics, source identity and capability consistency. All matrices are stored row-major; image-space multiplication uses column vectors. Finite numeric schema values are bounded to ±1e12 where applicable.

Paths resolve against the **package root**, including paths written inside the sidecar. They are not relative to the `relighting` directory. Reuse the existing `asset_path` / `AssetPath` boundary checks; absolute paths, traversal and resolved escapes are invalid. The sidecar has a fixed location; an existing directory or dangling symlink there is corruption, not absence.

Missing sidecar: load ordinary 3D. Invalid sidecar: fail the package load with `Invalid optional relighting/relighting.json: ...`. `ScenePackageLoader` validates it before constructing/importing the new scene. The asynchronous reconstruction manager does not publish a failed load to `ReconstructionSession`; the current scene remains active. Starting the executable directly with an invalid package has no previous scene to protect and reports startup failure.

## Image coordinates, color and camera

Stored file → EXIF orientation → ICC-to-sRGB (or explicit sRGB assumption without ICC) → existing white alpha composite in encoded sRGB → full-size canonical RGB8 → Lanczos thumbnail for analysis. Canonical and analysis PNGs carry no EXIF/ICC that could be applied a second time. Raw source bytes remain available separately. RGB8 conversion does **not** recover or retain high-bit-depth pixel precision. The white composite preserves prior pipeline behavior; it is not a new physically linear alpha operation.

Image origin is the top-left **edge**, X right, Y down; pixel `(i,j)` has continuous center `(i+0.5,j+0.5)`. With canonical dimensions `(Ws,Hs)` and analysis dimensions `(Wa,Ha)`:

```text
S = diag(Wa/Ws, Ha/Hs, 1)
pa = S ps
Ks = diag(Ws,Hs,1) Kn
Ka = diag(Wa,Ha,1) Kn = S Ks
```

`Kn` is the existing zero-skew normalized backend intrinsics. Pixel-space focal lengths and principal points use pixels; normalized K is dimensionless. For 1500×1000 → 512×341, `sx=0.3413333333333333`, `sy=0.341`. Use actual rounded sizes, not a single nominal scale. There is no half-pixel translation in this edge-origin convention. If using integer-center indices elsewhere, first add 0.5, transform, then subtract 0.5.

`storedToCanonical` represents all eight EXIF orientations before resize. Orientation 6 maps `(x,y)` to `(H-y,x)` and exchanges width/height. The available `sourceCamera` belongs to the saved reconstruction coordinate system: left-handed, Y up, camera forward +Z, currently identity camera-to-world. It comes from `GeometryPrediction`, not `scene.camera`. Synthetic Dummy intrinsics are available but `requiresCalibration=true`. Real backend intrinsics are marked `reconstruction-estimate`, never certified physical calibration; scale is separately `relative` or `metric`. Old packages without this evidence get `calibration-required`; even an existing editable scene camera is insufficient proof of the original camera.

## Limits and propagation

Input: single-frame JPEG/PNG, encoded bytes ≤128 MiB, ≤40,000,000 pixels, each dimension ≤16384. Canonical decode checks the conservative estimate `32 * pixels + 2 * encodedBytes <= 1536 MiB` before expensive transforms. This is a bound on an estimated working set, not an exact process RSS guarantee. Exceeding it fails explicitly; the anchor is never silently resized. Analysis keeps `--max-size` 16–2048 and never upscales. Python and C++ validate the same stored dimension/file limits; C++ hashes incrementally then decodes bounded CPU WIC pixels for validation and releases them immediately.

`remesh.py`, `segment.py`, `estimate_material.py` reuse `load_saved_geometry` and `write_appearance`. When a valid sidecar exists, source, processed analysis, original file and sidecar bytes are copied. There is no re-inference or re-capture of the source camera. For a legacy package only its processed image is available: the newly encoded processed PNG is also copied as a low-resolution anchor, with unknown earlier normalization and calibration-required. The original legacy directory is never overwritten; use a new destination.

`write_json_atomic` writes UTF-8 JSON into a sibling temporary file, flushes/fsyncs and replaces it. The existing exporter validates the entire staged package and only then renames the sibling package directory into the new/empty destination. This avoids exposing a partially assembled new package, but is not a general distributed transaction or a guarantee against every power-loss scenario. Sidecars are not mutated in place during repack.

## Diagnostics and verification

From the repository root, with the project Conda environment activated:

```powershell
python tools/reconstruction/anchor_examples.py generated/my-anchor16-tests
python tools/reconstruction/inspect_anchor.py generated/my-anchor16-tests/analysis512
python tools/reconstruction/inspect_anchor.py "generated/my-anchor16-tests/搬移目录/analysis256"
.\build\Debug\ValidateScenePackage.exe generated/my-anchor16-tests/analysis512 --appearance
python tools/reconstruction/inspect_anchor.py generated/my-anchor16-tests/bad-hash
# The previous command must exit 1 and explain the mismatch.
```

The diagnostic PNG fits both images to a display rectangle and magnifies corresponding top-left crops. This visualization does not alter or increase source information. Its JSON includes sizes, hashes, camera status, capabilities and three forward/inverse mapping samples. Successful reports have finite round-trip errors below `1e-9`. This verifies the encoded contract, not neural geometry accuracy or camera calibration.

For repository regression, the generated fixture directory defaults to `generated/prompt16`. Run the original-package baseline **before implementation changes** if comparing before/after captures; do not manufacture historical evidence afterward:

```powershell
.\tools\Build.ps1 -VisualStudioPath D:\VisualStudio
python -X utf8=0 tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
python -X utf8=1 tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
.\tools\Validate-Appearance.ps1
python tools/reconstruction/verify_appearance.py generated/prompt16 --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
```

The last command requires the retained baseline `generated/prompt16-before.bmp` and completed window runs. It regenerates diagnostic previews, checks valid/invalid packages in both languages, compares old Final pixels and writes `generated/prompt16-results.json`. The Python-only contract suite does not require those GPU captures. Windows symlink escape testing may be skipped when the account lacks symbolic-link privilege; lexical traversal/absolute path rejection still runs.

No GPU descriptor, shader, relighting buffer, independent source view or lighting estimator is introduced. CPU `AppearanceAnchor` is retained with the application session, while existing processed texture uploads, GPU barriers and frame fences keep their existing behavior.

**Stage 17 update:** the paragraph above records Stage 16's scope. The renderer now retains the validated canonical decode in an immutable `SourceObservation` and uploads it to an independent source-image pass. WorkMode is separate from the unchanged 3D RenderMode. See [ImageMode.md](ImageMode.md) for viewing, lifetime and validation details. The v1 manifest and relighting sidecar contract have not changed; no lighting estimator is introduced.
