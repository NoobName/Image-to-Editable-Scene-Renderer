"""Estimate intrinsic PBR maps over an existing reconstruction, preserving its geometry and regions."""
import argparse
from pathlib import Path


def main():
    if __package__:
        from .pipeline.material_options import add_material_arguments, material_backend_from_args
    else:
        from pipeline.material_options import add_material_arguments, material_backend_from_args
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--device", choices=("auto", "cpu", "cuda"), default="auto")
    parser.add_argument("--offline", action="store_true")
    add_material_arguments(parser, default="marigold")
    args = parser.parse_args()
    try:
        if __package__:
            from .pipeline.saved_prediction import load_saved_geometry
            from .pipeline.saved_segmentation import load_saved_segmentation
            from .pipeline.types import AnalysisResult, validate_analysis
            from .pipeline.material_rebuild import rebuild_geometry
            from .pipeline.scene_exporter import SceneExporter, check_destination
        else:
            from pipeline.saved_prediction import load_saved_geometry
            from pipeline.saved_segmentation import load_saved_segmentation
            from pipeline.types import AnalysisResult, validate_analysis
            from pipeline.material_rebuild import rebuild_geometry
            from pipeline.scene_exporter import SceneExporter, check_destination
        import numpy as np
        output = check_destination(args.output)
        image, geometry, report = load_saved_geometry(args.package)
        regions = load_saved_segmentation(args.package, image, geometry)
        backend = material_backend_from_args(args)
        material = backend.predict(image)
        labels = regions.labels if regions is not None else np.ones_like(geometry.depth, dtype=np.uint32)
        analysis = AnalysisResult(geometry.depth, geometry.normal, labels, material, geometry, regions)
        validate_analysis(image, analysis)
        # Rebuild with the saved geometric filtering options, not newly inferred geometry.
        mesh = rebuild_geometry(image, analysis, report)
        SceneExporter().export(output, image, analysis, mesh, {**report["backends"], "material": backend.name}, source_package=args.package)
        print(f"Material ScenePackage created: {output / 'scene.json'}")
        return 0
    except (ValueError, OSError, RuntimeError, KeyError, ImportError) as error:
        parser.exit(1, f"Material estimation failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
