"""Input JPEG/PNG -> interchangeable geometry estimation -> editable ScenePackage."""
import argparse
from pathlib import Path
import sys


def main() -> int:
    if __package__:
        from .pipeline.mesh_options import add_mesh_arguments, builder_from_args
        from .pipeline.segmentation_options import add_segmentation_arguments, backend_from_args
        from .pipeline.material_options import add_material_arguments, material_backend_from_args
        from .pipeline.lighting_options import add_lighting_arguments, lighting_backend_from_args
    else:
        from pipeline.mesh_options import add_mesh_arguments, builder_from_args
        from pipeline.segmentation_options import add_segmentation_arguments, backend_from_args
        from pipeline.material_options import add_material_arguments, material_backend_from_args
        from pipeline.lighting_options import add_lighting_arguments, lighting_backend_from_args
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="single JPEG or PNG input image")
    parser.add_argument("--output", required=True, type=Path, help="new/empty ScenePackage directory, relative to current working directory")
    parser.add_argument("--progress-file", type=Path, help="optional atomic progress JSON outside the output package")
    parser.add_argument("--job-id", default="", help="opaque progress identity supplied by the desktop application")
    parser.add_argument("--geometry-backend", choices=("dummy", "moge"), default="dummy")
    parser.add_argument("--backend", choices=("dummy",), help="deprecated stage-09 alias for --geometry-backend dummy")
    parser.add_argument("--device", choices=("auto", "cpu", "cuda"), default="auto")
    parser.add_argument("--checkpoint", type=Path, help="optional local official MoGe-2 normal checkpoint")
    parser.add_argument("--offline", action="store_true", help="use cached/local weights only; never download")
    parser.add_argument("--resolution-level", type=int, default=0, help="MoGe token budget level 0..9, independent of output image size")
    add_mesh_arguments(parser)
    add_segmentation_arguments(parser)
    add_material_arguments(parser)
    add_lighting_arguments(parser)
    parser.add_argument("--max-size", type=int, default=1024, help="maximum processed image edge, 16..2048 (default: 1024)")
    parser.add_argument("--depth", type=float, default=3.0, help="synthetic constant camera Z in meters, 0.1..100 (default: 3)")
    parser.add_argument("--fov", type=float, default=45.0, help="assumed vertical FOV in degrees, 10..120 (default: 45)")
    args = parser.parse_args()
    if __package__:
        from .pipeline.progress import ProgressReporter
    else:
        from pipeline.progress import ProgressReporter
    if args.progress_file and (args.progress_file.resolve() == args.output.resolve() or args.output.resolve() in args.progress_file.resolve().parents):
        parser.error("--progress-file must be outside the output package")
    reporter = ProgressReporter(args.progress_file, args.job_id)
    if args.backend and args.geometry_backend != "dummy":
        parser.error("--backend dummy conflicts with --geometry-backend moge")
    if sys.version_info < (3, 12):
        parser.exit(1, "Reconstruction requires Python 3.12+; run setup.ps1 to create .venv.\n")
    try:
        if __package__:
            from .pipeline.runner import ReconstructionPipeline
            from .pipeline.depth_backend import DummyDepthBackend
            from .pipeline.geometry_backend import DummyGeometryBackend
        else:
            from pipeline.runner import ReconstructionPipeline
            from pipeline.depth_backend import DummyDepthBackend
            from pipeline.geometry_backend import DummyGeometryBackend
    except ImportError as error:
        reporter.fail(error)
        parser.exit(1, f"Missing pipeline dependency: {error}\nRun tools/reconstruction/setup.ps1, then use .venv/Scripts/python.exe.\n")
    try:
        if args.geometry_backend == "moge":
            if args.depth != 3.0 or args.fov != 45.0:
                parser.error("--depth and --fov are Dummy-only; MoGe estimates depth and intrinsics")
            if __package__:
                from .pipeline.adapters.moge import MoGeGeometryBackend
            else:
                from pipeline.adapters.moge import MoGeGeometryBackend
            backend = MoGeGeometryBackend(args.device, args.checkpoint, args.offline, args.resolution_level)
        else:
            backend = DummyGeometryBackend(DummyDepthBackend(args.depth), fov_y_degrees=args.fov)
        pipeline = ReconstructionPipeline(geometry_backend=backend,geometry_builder=builder_from_args(args,args.fov),
            lighting_backend=lighting_backend_from_args(args),
            material_backend=material_backend_from_args(args),
            segmentation_backend=backend_from_args(args),min_region_pixels=args.min_region_pixels,max_regions=args.max_regions)
        result = pipeline.run(args.input, args.output, args.max_size, stage_event=reporter.stage)
        reporter.finish()
        print(f"ScenePackage created: {result / 'scene.json'}")
        print(f"Geometry: {args.geometry_backend}; segmentation: {args.segmentation_backend}; material: {args.material_backend}. No training performed.")
        return 0
    except Exception as error:
        reporter.fail(error)
        parser.exit(1, f"Reconstruction failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
