from .lighting_backend import ManualLightingBackend
from .lighting_solver import RobustDirectionalAmbientBackend


def add_lighting_arguments(parser):
    parser.add_argument('--lighting-backend', choices=('robust-directional-ambient', 'intrinsic-assisted', 'manual-test'), default='robust-directional-ambient')
    parser.add_argument('--light-direction', type=float, nargs=3, default=(.2, -.3, 1), help='manual light TRAVEL direction, LH camera')
    parser.add_argument('--direct-rgb', type=float, nargs=3, default=(1, 1, 1), help='manual relative coefficients after fixed median normalization')
    parser.add_argument('--ambient-rgb', type=float, nargs=3, default=(.2, .2, .2))


def lighting_backend_from_args(args):
    if args.lighting_backend == 'intrinsic-assisted':
        from .diffuse_backend import IntrinsicAssistedBackend
        return IntrinsicAssistedBackend()
    return RobustDirectionalAmbientBackend() if args.lighting_backend == 'robust-directional-ambient' else ManualLightingBackend(args.light_direction, args.direct_rgb, args.ambient_rgb)
