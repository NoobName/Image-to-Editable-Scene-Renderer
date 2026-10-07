"""Official Lighting checkpoint, pinned from the HF repository manifest on 2026-10-06."""
from pathlib import Path
from .marigold_config import CONFIG_FILES
ROOT=Path(__file__).resolve().parents[2]
MODEL_ID='prs-eth/marigold-iid-lighting-v1-1'
MODEL_REVISION='08c3930bb641abf786ba44ce92547507ebefbc16'
WEIGHT_HASHES={
    'text_encoder/model.fp16.safetensors':'bc1827c465450322616f06dea41596eac7d493f4e95904dcb51f0fc745c4e13f',
    'unet/diffusion_pytorch_model.fp16.safetensors':'8c2e8da73793181f87c9c7a752a939d6f6834104e9854bf1c53c6b37de3594d9',
    'vae/diffusion_pytorch_model.fp16.safetensors':'3e4c08995484ee61270175e9e7a072b66a6e4eeb5f0c266667fe1f45b90daf9a'}

def checkpoint_path(offline=False):
    import hashlib
    from huggingface_hub import snapshot_download
    path=Path(snapshot_download(MODEL_ID,revision=MODEL_REVISION,allow_patterns=[*CONFIG_FILES,*WEIGHT_HASHES,'README.md','LICENSE*'],cache_dir=str(ROOT/'.cache/huggingface'),local_files_only=offline,max_workers=3))
    for name in CONFIG_FILES:
        if not (path/name).is_file():raise RuntimeError(f'Missing intrinsic config: {name}')
    for name,expected in WEIGHT_HASHES.items():
        with (path/name).open('rb') as f:
            if hashlib.file_digest(f,'sha256').hexdigest()!=expected:raise RuntimeError(f'Intrinsic SHA256 mismatch: {name}')
    return path
