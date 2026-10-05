# ScenePackage v1 sample

Run from the repository root:

```powershell
.\build\Debug\ImageSceneRenderer.exe --package "assets/ScenePackage"
.\build\Debug\ValidateScenePackage.exe "assets/ScenePackage"
```

The left instance preserves MaterialLab's glTF materials. The right instance uses a
package-wide material with separate roughness and metallic R-channel maps. Both
reference the same self-contained GLB. Camera, two lights, HDRI, and Look settings
come from scene.json. Original asset sources are `tools/GenerateAssets.cpp` and
`tools/GenerateEnvironments.cpp`; these are project-generated test assets.

Reproduce in an empty destination (Python 3.10+, standard library only):

```powershell
python tools/reconstruction/scene_package.py example "generated/MyScenePackage"
```

`masks/segmentation.png` is a one-pixel label-format sample (ID 1), not an AI result
or a segmentation of the rendered view. Optional EXR references are intentionally
absent: this stage transports existing EXR files but does not create/decode them.
`debug/.gitkeep` keeps the required empty directory in version control.

The shared schema is `schemas/scene-package.schema.json`. All internal paths are
relative to this directory; the package can be moved intact to another location.
