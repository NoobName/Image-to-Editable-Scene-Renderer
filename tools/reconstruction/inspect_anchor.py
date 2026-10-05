"""Validate an optional Appearance Anchor and write a source/analysis diagnostic, without models."""
import argparse
import json
from pathlib import Path
from appearance_contract import load_extension
from scene_package import load_package
from pipeline.appearance_debug import write_diagnostic


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package",type=Path)
    parser.add_argument("--output",type=Path,help="diagnostic PNG; JSON report is written beside it")
    args = parser.parse_args()
    try:
        root = args.package if args.package.is_dir() else args.package.parent
        load_package(args.package)
        metadata = load_extension(root)
        result = write_diagnostic(root,metadata,args.output) if metadata else {"status":"absent","3dCompatible":True,"sourceCameraAvailable":False}
        print(json.dumps(result,indent=2,ensure_ascii=False))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1,f"Appearance diagnostics failed: {error}\n")


if __name__ == "__main__":
    main()
