"""Saved ScenePackage predictions -> new textured scene_mesh.glb; no model required."""
import argparse
from pathlib import Path


def main():
    if __package__:
        from .pipeline.mesh_options import add_mesh_arguments, builder_from_args
    else:
        from pipeline.mesh_options import add_mesh_arguments, builder_from_args
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="stage-10+ package with debug/geometry.npz and textures/base_color.png")
    parser.add_argument("--output", type=Path, required=True, help="new/empty output package; input remains unchanged")
    parser.add_argument("--geometry-source", choices=("point-map","depth"), default="point-map")
    add_mesh_arguments(parser)
    args = parser.parse_args()
    try:
        if __package__:
            from .pipeline.saved_prediction import remesh_saved
        else:
            from pipeline.saved_prediction import remesh_saved
        output = remesh_saved(args.package,args.output,builder_from_args(args),args.geometry_source)
        print(f"ScenePackage created without inference: {output / 'scene.json'}")
        return 0
    except (ValueError, OSError, RuntimeError, KeyError, ImportError) as error:
        parser.exit(1,f"Remeshing failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
