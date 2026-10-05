"""Offline lighting-only process: reuse fixed saved predictions, publish a NEW ScenePackage."""
import argparse
from pathlib import Path
from export_analysis import export_saved
from pipeline.lighting_options import add_lighting_arguments, lighting_backend_from_args
from pipeline.progress import ProgressReporter, OFFLINE_STAGES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--progress-file', type=Path)
    parser.add_argument('--job-id', default='')
    add_lighting_arguments(parser)
    args = parser.parse_args()
    if args.progress_file and (args.progress_file.resolve() == args.output.resolve() or args.output.resolve() in args.progress_file.resolve().parents):
        parser.error('Progress file must be outside the output package')
    reporter = ProgressReporter(args.progress_file, args.job_id, OFFLINE_STAGES)
    try:
        result = export_saved(args.package, args.output, lighting_backend_from_args(args), reporter.stage)
        reporter.finish(); print(result)
        return 0
    except Exception as error:
        reporter.fail(error); print(f'Lighting estimation failed: {error}')
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
