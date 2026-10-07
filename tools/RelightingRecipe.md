# Relighting Recipe v1 / native export

Stage 29 adds a separate image-edit recipe. ScenePackage v1, its canonical source, and 3D scene edits remain separate. C++ does not require a Python runtime for recipe loading or export.

```powershell
./tools/Build.ps1 -VisualStudioPath D:/VisualStudio -BuildDirectory generated/build-prompt29
./tools/Run-Smoke.ps1 -BuildDirectory generated/build-prompt29 -Package generated/prompt21-detail -WorkMode image -ImageView relighted -Frames 90 -LogName recipe-save -SaveRecipe generated/my-recipe.json -ExportImage generated/my-native-export
./generated/build-prompt29/Debug/ImageSceneRenderer.exe --recipe generated/my-recipe.json --ui
./tools/Run-Smoke.ps1 -BuildDirectory generated/build-prompt29 -Recipe generated/my-recipe.json -Frames 90 -LogName recipe-replay -ExportImage generated/my-replay
```

The example package is previously generated local evidence. Supply any validated package with an appearance anchor on a fresh checkout. Generate analytic fixtures without models using `python tools/reconstruction/cast_shadow_examples.py --output generated/my-fixtures`.

Both work modes expose an Inspector **Recipe / Export** tab. Open recipe loads/validates a candidate asynchronously and commits its source, GPU resources and edit state together. Cancel recipe load, failure, or a changed scene revision retains the previous document. Save new rejects existing files; Replace recipe explicitly permits replacement. CLI uses `--save-recipe FILE` and optional `--replace-recipe`. Save outside the immutable source package, with `.json` extension and an existing parent directory.

`schemas/relighting-recipe.schema.json` defines version 1, renderer revision `image-relighting-29-v1` and parameter contract `bounded-response-v1`. It stores source/anchor identity, package/analysis/lighting/intrinsic/shadow manifest fingerprints, analysis/backend provenance, explicit manual source baseline revision/cache validity, target plus global gain, every response parameter, stable region IDs/labels/weights, imported protection mask reference, display EV and confidence-overlay settings. UI and CLI share `RecipeState`/`ApplyRecipeState`; unknown fields/versions, stale hashes, missing dependencies, non-unit directions and invalid ranges fail explicitly.

Package references are relative to the recipe and may address a sibling with `../`. Move the enclosing tree together. Internal package assets retain existing canonical-root validation. Imported PNG masks are copied to immutable, hash-named `recipe-assets` entries under the recipe directory. Absolute/drive/URI references are rejected. This is not a general 3D scene save format; camera/object edits and display fit/pan/zoom are not serialized.

`--export-image NEW_DIRECTORY` requires a finite `--frames` run. The UI export button uses the same implementation. Exports are native source dimensions, with no UI, letterbox, comparison line or confidence overlay. Explicit limits: 8192 pixels per edge, 8 MiPixels total; additional readback/CPU budget <=768 MiB, excluding existing renderer resources. No implicit downscale to analysis resolution. Export directories must be new. Staging and one directory rename prevent partially published results.

Outputs:

- `original.png`: canonical/legacy source RGB, unchanged.
- `result.png`: display-referred sRGB RGB8 with image display EV applied once; no 3D Look/ACES/Bloom.
- `result.dds`: pre-display-EV linear-sRGB display-referred RGBA32F. **Not recovered HDR radiance.** `export.json` records EV and parameters.
- `old-shading`, `new-shading`, `ratio`, `quality`: restricted DX10, single mip/slice uncompressed float DDS, plus bounded PNG previews. Ratio/quality PNGs clip to 0..1; use DDS for numeric values.
- `confidence.dds/.png`: R32_FLOAT effective confidence from ratio alpha.
- `export.json`: source identity, exact state, sizes, finite checks and tracked resource states.

Only explicit exports/captures read back GPU resources. State is restored after COPY_SOURCE and data is mapped after the submission fence. Old scene resources follow the existing retirement mechanism.

Verification: `tools/Validate-Recipe.ps1`, `tools/reconstruction/verify_recipe.py OUTPUT`, `RecipeTests` (CTest CPU contract) and `RecipeTests.exe FIXTURE OUTPUT` (portable real-package/Chinese-path/locked-file/mask integration). Existing ImGui events now also round-trip their edited state through the same recipe functions. Actual stage 29 measurements: native no-op 0 LSB, replay/UI/Release/transaction float exact, WARP maximum linear difference 5.96e-8. See local ignored teaching/validation records for detailed evidence and limitations. No physical power-loss or network-share durability guarantee is claimed.
## Optional 33 compatibility update

New saves use Recipe v2 (`image-relighting-33-v2`, `bounded-response-fog-v2`) with independent `state.imageFog`. Old v1 files remain readable and reset fog to OFF. V2 is deliberately rejected by pre-33 executables; the shared strict schema enforces the revision combination. Atomic save/load, source identity, relative paths and ScenePackage v1 are unchanged. Additional native export buffers are `fog-distance.dds` and `pre-fog.dds`; see [ImageFog](ImageFog.md) for units, linear composition and limits. The original v1 design above records the preceding stage.

## Manual point lights compatibility update

Recipes with image point lights use v3 (`image-point-lights-v3`, `bounded-response-points-v3`). `state.imagePointLights` contains at most four records: unique positive `id`, `position` (LH source-camera XYZ, Z >= 0.01), linear RGB `color` in [0,1], relative `intensity` in [0,10000], finite `range` in [0.01,10000], and `enabled`. Coordinates are bounded to +/-100000 in point-map units, which are not necessarily metres. A point position is independent of the editable 3D camera and Scene lights.

The source image and source lighting are immutable during point editing. Only calculated new diffuse shading includes these lights. The existing ratio/confidence/protection composition remains active; point lights do not cast new shadows or add image specular. Target global gain also scales point intensity. Reset directional/environment lighting leaves point records intact; use the explicit point clear/delete controls. Recipe export uses the same point state as the UI.

V1/v2 still load and clear previously held point lights. Saves without point records continue to use v2. Nonempty point lists cannot be labelled v1/v2; unknown fields, invalid bounds and duplicate IDs are rejected before committing a candidate. Older executables reject v3. Existing atomic publication, hash checks and relative package paths are reused. 3D point edits remain session-only. Directional-only inverse optimization and neural refinement explicitly reject point-light recipes rather than treating the extra illumination as their directional guidance.
