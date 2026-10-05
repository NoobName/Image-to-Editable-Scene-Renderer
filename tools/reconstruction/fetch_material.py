"""Download the pinned official Marigold IID appearance fp16 safetensors (~2.58 GB)."""
import argparse
from pipeline.adapters.marigold_config import checkpoint_path

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    print(checkpoint_path(args.offline))
