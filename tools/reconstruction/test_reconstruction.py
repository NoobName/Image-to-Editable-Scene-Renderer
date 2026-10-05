"""Run CPU/format tests; optionally validate generated packages through C++ executables."""
import argparse
from pathlib import Path
import unittest
from tests import test_pipeline, test_stages, test_geometry_backend, test_grid_mesh, test_segmentation, test_material, test_progress, test_appearance
from tests.support import make_input
from tests import test_analysis, test_lighting


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validator", type=Path, action="append", default=[], help="C++ ValidateScenePackage.exe; may repeat for Debug/Release")
    parser.add_argument("--make-input", type=Path, help="write a standalone asymmetric test JPEG/PNG, then exit")
    args = parser.parse_args()
    if args.make_input:
        print(make_input(args.make_input))
        return 0
    test_pipeline.VALIDATORS = [path.resolve(strict=True) for path in args.validator]
    suite = unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromModule(module) for module in (test_stages, test_pipeline, test_geometry_backend, test_grid_mesh, test_segmentation, test_material, test_progress, test_appearance, test_analysis, test_lighting))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
