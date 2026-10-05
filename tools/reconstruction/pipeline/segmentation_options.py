"""CLI composition; model imports remain lazy and adapter-local."""


def add_segmentation_arguments(parser,default="dummy"):
    parser.add_argument("--segmentation-backend",choices=("dummy","sam2"),default=default)
    parser.add_argument("--segmentation-prompts",help="named SAM point/box prompts JSON; omit for automatic masks")
    parser.add_argument("--sam2-points-per-side",type=int,default=16,help="automatic mask sampling grid, 4..32")
    parser.add_argument("--min-region-pixels",type=int,default=64)
    parser.add_argument("--max-regions",type=int,default=32,help="including remaining Background, 2..128")


def backend_from_args(args):
    if args.segmentation_backend=="dummy":
        if args.segmentation_prompts:
            raise ValueError("Named segmentation prompts require --segmentation-backend sam2")
        from .segmentation_backend import DummySegmentationBackend
        return DummySegmentationBackend()
    from .adapters.sam2 import Sam2SegmentationBackend
    return Sam2SegmentationBackend(args.device,args.offline,args.segmentation_prompts,args.sam2_points_per_side)
