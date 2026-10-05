"""Add object segmentation to an existing prediction package without rerunning geometry."""
import argparse
from pathlib import Path


def main():
    if __package__:
        from .pipeline.segmentation_options import add_segmentation_arguments,backend_from_args
        from .pipeline.mesh_options import add_mesh_arguments,builder_from_args
    else:
        from pipeline.segmentation_options import add_segmentation_arguments,backend_from_args
        from pipeline.mesh_options import add_mesh_arguments,builder_from_args
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package",type=Path)
    parser.add_argument("--output",type=Path,required=True,help="new/empty package directory")
    parser.add_argument("--device",choices=("auto","cpu","cuda"),default="auto")
    parser.add_argument("--offline",action="store_true")
    add_segmentation_arguments(parser,default="sam2")
    add_mesh_arguments(parser)
    args = parser.parse_args()
    try:
        if __package__:
            from .pipeline.saved_prediction import load_saved_geometry
            from .pipeline.scene_decomposer import resolve_regions
            from .pipeline.scene_exporter import SceneExporter,check_destination
            from .pipeline.types import AnalysisResult,MaterialPrediction
        else:
            from pipeline.saved_prediction import load_saved_geometry
            from pipeline.scene_decomposer import resolve_regions
            from pipeline.scene_exporter import SceneExporter,check_destination
            from pipeline.types import AnalysisResult,MaterialPrediction
        output = check_destination(args.output)
        image,geometry,report = load_saved_geometry(args.package)
        backend = backend_from_args(args)
        segmentation = resolve_regions(image,backend.predict(image),geometry,
                                       min_pixels=args.min_region_pixels,max_regions=args.max_regions)
        if __package__:
            from .pipeline.material_assets import load_saved_material
        else:
            from pipeline.material_assets import load_saved_material
        analysis = AnalysisResult(geometry.depth,geometry.normal,segmentation.labels,load_saved_material(args.package,image),geometry,segmentation)
        mesh = builder_from_args(args).build(image,analysis)
        SceneExporter().export(output,image,analysis,mesh,{**report["backends"],"segmentation":backend.name})
        print(f"Exported {len(segmentation.regions)} independently editable regions: {output/'scene.json'}")
        return 0
    except (ValueError,OSError,RuntimeError,KeyError,ImportError) as error:
        parser.exit(1,f"Segmentation failed: {error}\n")


if __name__=="__main__":
    raise SystemExit(main())
