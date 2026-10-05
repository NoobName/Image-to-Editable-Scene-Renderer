"""Pinned official appearance checkpoint; downloads never execute model repository code."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODEL_ID = "prs-eth/marigold-iid-appearance-v1-1"
MODEL_REVISION = "e7280a0a0fc5a0df0b36050882b3d8b77da22fd9"
WEIGHT_HASHES = {
    "text_encoder/model.fp16.safetensors": "bc1827c465450322616f06dea41596eac7d493f4e95904dcb51f0fc745c4e13f",
    "unet/diffusion_pytorch_model.fp16.safetensors": "6c7ab00d751edc8ac26a56d6d5bdcef600f2577b7ec708bea9cbac3fb12eda39",
    "vae/diffusion_pytorch_model.fp16.safetensors": "3e4c08995484ee61270175e9e7a072b66a6e4eeb5f0c266667fe1f45b90daf9a",
}
CONFIG_FILES = ("model_index.json", "scheduler/scheduler_config.json", "text_encoder/config.json",
                "tokenizer/merges.txt", "tokenizer/special_tokens_map.json", "tokenizer/tokenizer_config.json",
                "tokenizer/vocab.json", "unet/config.json", "vae/config.json")


def checkpoint_path(offline=False):
    import hashlib
    from huggingface_hub import snapshot_download
    path = Path(snapshot_download(MODEL_ID, revision=MODEL_REVISION,
        allow_patterns=[*CONFIG_FILES, *WEIGHT_HASHES, "README.md", "LICENSE*"],
        cache_dir=str(ROOT / ".cache/huggingface"), local_files_only=offline, max_workers=3))
    for name in CONFIG_FILES:
        if not (path / name).is_file():
            raise RuntimeError(f"Missing Marigold configuration: {name}; run fetch_material.py")
    for name, expected in WEIGHT_HASHES.items():
        with (path / name).open("rb") as stream:
            if hashlib.file_digest(stream, "sha256").hexdigest() != expected:
                raise RuntimeError(f"Marigold SHA256 mismatch: {name}")
    return path
