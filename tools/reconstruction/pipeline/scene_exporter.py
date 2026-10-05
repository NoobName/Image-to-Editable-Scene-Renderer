"""Publish a complete ScenePackage through files only; never invokes the renderer."""
from importlib.metadata import version
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from PIL import Image
try:
    from ..scene_package import create_package, write_package, load_package, asset_path, copy_asset
except ImportError:
    from scene_package import create_package, write_package, load_package, asset_path, copy_asset
from .types import InputImage, AnalysisResult, GeometryResult
from .gltf_writer import write_glb
from .auxiliary_writer import write_auxiliary
from .object_exporter import export_objects
from .material_estimation_backend import MaterialEstimate
from .material_assets import write_material_assets, material_glb_options
from .appearance_assets import write_appearance


def check_destination(output: Path) -> Path:
    output = Path(output).absolute()
    if output.is_symlink():
        raise ValueError("Output must not be a symbolic link")
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise FileExistsError(f"Output must be a new or empty directory: {output}")
    return output.resolve()


class SceneExporter:
    def export(self, output: Path, image: InputImage, analysis: AnalysisResult,
               geometry: GeometryResult, backend_names: dict[str, str], source_package: Path | None = None) -> Path:
        output = check_destination(output)
        source_manifest = load_package(source_package) if source_package is not None else None
        output.parent.mkdir(parents=True, exist_ok=True)
        # This owned staging directory is the only tree automatically removed on failure.
        # Existing output assets are never recursively deleted or overwritten.
        with TemporaryDirectory(prefix=f".{output.name}-staging-", dir=output.parent) as staging:
            root = create_package(Path(staging) / "package")
            appearance = write_appearance(root, image, analysis.geometry)
            # Keep the legacy image path as source-photo compatibility, never bind it as estimated albedo.
            Image.fromarray(image.rgb).save(root / "textures/base_color.png")
            estimated = isinstance(analysis.material, MaterialEstimate)
            material_override = write_material_assets(root, image, analysis.material) if estimated else None
            color_path = "textures/object_albedo.png" if estimated else "textures/base_color.png"
            write_glb(root / "meshes/scene_mesh.glb", geometry, (root / color_path).read_bytes(),
                      **material_glb_options(root, analysis.material, True))
            auxiliary = write_auxiliary(root, analysis, geometry, original_rgb=image.rgb)
            from .analysis_assets import write_analysis_maps
            write_analysis_maps(root, image, analysis, appearance)
            if analysis.lighting is not None:
                from .lighting_assets import write_lighting_assets
                write_lighting_assets(root, image, appearance, analysis.lighting)
            objects,decomposition = export_objects(root,image,analysis,geometry)
            if material_override:
                for item in objects:
                    if "mesh" in item:
                        item["material"] = material_override.copy()
            prediction = analysis.geometry
            valid_depth = analysis.depth[prediction.valid_mask] if prediction is not None else analysis.depth
            z_min, z_max = float(valid_depth.min()), float(valid_depth.max())
            is_dummy = prediction is None or prediction.metadata.get("dummy", False)
            manifest = {
                "version": 1, "coordinateSystem": "left-handed-y-up", "units": "meters",
                "camera": {"position": [0, 0, 0], "target": [0, 0, (z_min+z_max)/2],
                           "fovYDegrees": geometry.fov_y_degrees, "aspect": image.width/image.height,
                           "near": max(.0001, z_min/100), "far": max(1, z_max*2)},
                "environment": {},
                "lights": [{"type": "directional", "direction": [.2, -.3, 1], "color": [1, 1, 1], "intensity": 3}],
                # Keep the embedded GLB material; package material objects replace it entirely.
                "objects": objects,
                "look": {"exposure": 0, "tone-mapping": "aces"}, "auxiliary": auxiliary}
            if source_manifest is not None:
                # Material-only updates retain stored scene settings and object poses.
                for key in ("camera", "environment", "lights", "look"):
                    manifest[key] = source_manifest[key]
                old_objects = {obj.get("id", "legacy:"+obj["name"]): obj for obj in source_manifest["objects"]}
                for item in objects:
                    previous = old_objects.get(item["id"])
                    if previous is None and len(objects)==len(old_objects)==1:
                        previous = next(iter(old_objects.values()))
                    if previous is None:
                        raise ValueError("Material update cannot match reconstructed objects to stored scene IDs")
                    for key in ("transform", "visible"):
                        item[key] = previous[key]
                if "hdri" in manifest["environment"]:
                    relative = manifest["environment"]["hdri"]
                    copy_asset(root, asset_path(source_package, relative, "textures", (".hdr",)), relative)
            report = {
                "pipeline_version": 5, "dummy": is_dummy, "backends": backend_names,
                "material_estimation": analysis.material.metadata if estimated else {"albedo_source": "legacy-photo-placeholder"},
                "decomposition":decomposition,
                "geometry_estimation": prediction.metadata if prediction is not None else {},
                "scale_type": prediction.scale_type if prediction is not None else "synthetic",
                "valid_fraction": float(prediction.valid_mask.mean()) if prediction is not None else 1.,
                "input": {"name": image.path.name, "sha256": image.source_sha256,
                          "source_size": list(image.source_size), "processed_size": [image.width, image.height],
                          "color_space": "sRGB", "icc_applied": image.color_profile_applied, "alpha_background": "white"},
                "geometry": {"vertices": len(geometry.positions), "triangles": len(geometry.indices),
                             "depth_range_meters": [z_min, z_max], "fov_y_degrees": geometry.fov_y_degrees,
                             "mesh_filtering": geometry.diagnostics},
                "dependencies": {name: version(name) for name in ("numpy", "Pillow", "OpenEXR")},
                "limitations": (["No inference or camera calibration; geometry is Dummy."] if is_dummy else
                                ["Single-image estimated geometry and scale are not ground truth.",
                                 "Confidence is validity, not calibrated geometric accuracy."]) +
                                ["Only visible image surfaces; no hidden geometry completion.",
                                "Region categories are prompt-provided or geometry heuristics; SAM masks are class-agnostic.",
                                "Intrinsic material estimates can retain lighting errors; fallback properties are recorded separately." if estimated else
                                "Legacy photo placeholder contains original lighting; it is not intrinsic albedo.",
                                "Renderer viewport aspect can differ; use Frame all to fit the surface."]}
            (root / "debug/reconstruction.json").write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False)+"\n", encoding="utf-8")
            write_package(root, manifest)  # Reuses the v1 shared schema and file validation.
            from .appearance_debug import write_diagnostic
            write_diagnostic(root, appearance)
            # Recheck after writing: a concurrently populated output is never removed.
            check_destination(output)
            if output.exists():
                output.rmdir()  # Empty directory only; no recursive removal.
            root.rename(output)
        return output
