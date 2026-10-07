"""Upgrade saved arrays into a NEW package, preserving meshes, source and EXR/NPZ bytes."""
import argparse
from pathlib import Path
from tempfile import TemporaryDirectory
import shutil
import numpy as np
from pipeline.saved_prediction import load_saved_geometry
from pipeline.saved_segmentation import load_saved_segmentation
from pipeline.material_assets import load_saved_material
from pipeline.appearance_assets import write_appearance
from pipeline.analysis_assets import write_analysis_maps
from pipeline.scene_exporter import check_destination
from pipeline.types import AnalysisResult
from scene_package import load_package


def export_saved(package, output, lighting_backend=None, stage_event=None):
    package = Path(package).resolve(strict=True)
    output = check_destination(Path(output))
    if output.resolve().is_relative_to(package):
        raise ValueError('New analysis package must be outside the input package')
    image, geometry, report = load_saved_geometry(package)
    segmentation = load_saved_segmentation(package, image, geometry)
    labels = segmentation.labels if segmentation is not None else np.ones_like(geometry.depth, dtype=np.uint32)
    analysis = AnalysisResult(geometry.depth, geometry.normal, labels, load_saved_material(package, image), geometry, segmentation)
    event = stage_event or (lambda *args: None)
    estimate = None
    if lighting_backend is not None:
        from pipeline.material_estimation_backend import MaterialEstimate
        if not isinstance(analysis.material, MaterialEstimate):
            raise ValueError('Lighting unavailable: saved package has no estimated albedo; estimate materials first')
        from pipeline.lighting_backend import make_lighting_input
        if hasattr(lighting_backend, 'bind_package'):
            from appearance_contract import load_extension
            lighting_backend.bind_package(package, load_extension(package))
        event('lighting', 'running', lighting_backend.name)
        estimate = lighting_backend.predict(make_lighting_input(image, analysis))
        event('lighting', 'complete', estimate.fit['status'])
        event('export', 'running', 'Publishing lighting evidence in a new package')
    output.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix=f".{output.name}-analysis-", dir=output.parent) as staging:
        target = Path(staging) / "package"
        # Refuse links in the source tree; no side effects on another directory or hidden escape.
        if any(p.is_symlink() for p in package.rglob("*")):
            raise ValueError("Analysis upgrade does not copy symbolic links")
        shutil.copytree(package, target)
        appearance = write_appearance(target, image, geometry)
        write_analysis_maps(target, image, analysis, appearance)
        from pipeline.shadow_assets import invalidate_shadow
        invalidate_shadow(target)  # Geometry/light versions changed in this private output snapshot.
        if estimate is not None:
            from pipeline.lighting_assets import write_lighting_assets
            write_lighting_assets(target, image, appearance, estimate)
        load_package(target)
        check_destination(output)
        if output.exists():
            output.rmdir()
        target.rename(output)
    if estimate is not None:
        event('export', 'complete', 'ScenePackage published')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(export_saved(args.package, args.output))


if __name__ == "__main__":
    main()
