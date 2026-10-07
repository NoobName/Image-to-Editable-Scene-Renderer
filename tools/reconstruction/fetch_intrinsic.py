"""Fetch only pinned safetensors/configs; never execute remote model code."""
from pipeline.adapters.intrinsic_config import checkpoint_path
if __name__=='__main__':print(checkpoint_path())
