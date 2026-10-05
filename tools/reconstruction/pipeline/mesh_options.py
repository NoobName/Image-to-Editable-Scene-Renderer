"""Shared mesh controls for inference and offline remeshing."""


def add_mesh_arguments(parser):
    parser.add_argument("--grid-size", type=int, default=None,
                        help="optional coarse grid longest edge, 2..257; default: every valid pixel")
    parser.add_argument("--confidence-threshold", type=float, default=.5)
    parser.add_argument("--depth-edge-threshold", type=float, default=.15, help="maximum relative depth span per triangle")
    parser.add_argument("--depth-edge-meters", type=float, default=0, help="additional absolute depth difference limit; 0 disables")
    parser.add_argument("--max-edge-stretch", type=float, default=8, help="3D edge / projected pixel footprint limit, >=1")
    parser.add_argument("--max-edge-meters", type=float, default=0, help="additional absolute 3D edge limit; 0 disables")
    parser.add_argument("--max-triangle-area", type=float, default=0, help="additional triangle area limit in square meters; 0 disables")


def builder_from_args(args, fov=45):
    from .geometry_builder import GeometryBuilder
    return GeometryBuilder(args.grid_size, fov, args.confidence_threshold, args.depth_edge_threshold,
                           depth_edge_meters=args.depth_edge_meters, max_edge_stretch=args.max_edge_stretch,
                           max_edge_meters=args.max_edge_meters, max_triangle_area=args.max_triangle_area)
