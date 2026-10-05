"""Material CLI composition; model imports stay lazy."""


def add_material_arguments(parser, default="neutral"):
    parser.add_argument("--material-backend", choices=("neutral", "marigold"), default=default)
    parser.add_argument("--material-resolution", type=int, default=512)
    parser.add_argument("--material-steps", type=int, default=4)
    parser.add_argument("--material-ensemble", type=int, default=3)
    parser.add_argument("--material-seed", type=int, default=13)


def material_backend_from_args(args):
    if args.material_backend == "neutral":
        from .material_estimation_backend import NeutralMaterialBackend
        return NeutralMaterialBackend()
    from .adapters.marigold import MarigoldMaterialBackend
    return MarigoldMaterialBackend(args.device, args.offline, args.material_resolution,
                                   args.material_steps, args.material_ensemble, args.material_seed)
