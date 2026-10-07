# Dual-mode image workspace (Prompt 28)

This stage adds editing workflow around the existing image relighting passes. It does not introduce reference inference, fog, reflection reconstruction or a new lighting solver. ScenePackage v1, sidecars, render-mode integers and image-view CLI keys are unchanged.

```powershell
./tools/Build.ps1 -VisualStudioPath D:/VisualStudio -BuildDirectory generated/build-prompt28
./generated/build-prompt28/Debug/ImageSceneRenderer.exe --package "generated/scene26-final-indoor" --work-mode image --image-view relighted --ui
```

Use your own validated package if the local example is absent. Old packages without an anchor still open in 3D; the image mode switch is disabled.

* Left: immutable Original/source thumbnail; Reference explicitly remains empty and unanalysed.
* Center: Image Relighting or 3D Scene. Image view has grouped debug menus, Fit, Single / Side by side / Wipe. Wheel zooms around the pointer; MMB pans; LMB selects a source label, or moves the wipe divider. The yellow sun edits target lighting in source-camera coordinates. Blue indicates source lighting.
* Right: Lighting separates explicit source calibration from target edits; Image / Protection provides display exposure, low-confidence overlay and stable-region protection; Sources exposes dimensions, mapping, camera and provenance. Scroll for additional response/shadow controls. 3D retains hierarchy, mesh picking, Transform, Material and Look.

Region protection: select a region in Single/Side by side, set Protection weight, then Apply region protection. This edits only the relighting weight. Clear session protection removes these edits but preserves any imported `--protection-mask`. Region identities come from the immutable package observation, including regions with no mesh triangles. Label-only packages use sourceId + label ID. No mask is written to disk; successful package replacement resets session edits, while failed/cancelled replacement preserves them. Switching modes preserves each mode's editing state.

Stage 29 extension: the stage 28 editing actions above still only change session state. Explicit **Recipe / Export** saves stable region weights and an immutable copy of imported masks in a separate [Relighting Recipe](RelightingRecipe.md); loading it restores image edits without modifying source pixels or ScenePackage v1.

Cast Old/New Shadow Map are square light-space textures. They keep a square fit and temporarily disable source picking/comparison instead of treating light-map coordinates as photograph coordinates. The previous comparison preference resumes in image-space views.

Global intensity multiplies both target direct and ambient base gains; it affects Calculated New Shading while preserving source. Reset target to source also resets this multiplier to 1. Exposure is a separate display-only linear gain `2^EV` applied to Relighted (including the Final with Paired Cast Shadows alias), followed by the existing sRGB encoding. It never changes source lighting, numerical targets or debug views. Red overlay marks effective ratio confidence below the selected threshold; this heuristic is not an uncertainty probability or a separate cast-shadow confidence. Use the shadow confidence view to inspect that evidence.

The original texture is borrowed via one reserved ImGui UNORM SRV (slot 62). It retains encoded RGB8 values; no extra sRGB decode is applied by ImGui. Rebinding on package replacement waits for prior UI submissions; normal edits allocate no new descriptors. The full-source float protection texture adds at most 64 MiB inside the existing 16M-pixel composition limit. An explicit Apply builds/uploads the mask; a frame owns upload memory until its fence completes. A large anchor can therefore cause a short CPU upload cost on Apply. Pan/zoom changes ImGui display geometry only; computed debug views enlarge the viewport render target and do not create additional analysis detail. Original view directly samples the canonical texture.

Validation entry points:

```powershell
./tools/Validate-ImageWorkspace.ps1
conda run -n image-scene-renderer python -X utf8=1 tools/reconstruction/verify_image_workspace.py
```

The matrix assumes local stage27 fixtures and the stage26 indoor package. It uses real finite-frame HWND runs; ImageWorkspaceTests uses actual ImGui input events without a desktop backend. Automated state scenarios in Run-Smoke are separately identified and are not presented as manual mouse操作. Numerical readback includes `protection.bin`, checks finite values, and records SRV → COPY_SOURCE → SRV states. Full local teaching/validation stays under ignored `docs/`.
