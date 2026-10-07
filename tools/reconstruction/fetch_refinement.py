"""Explicit optional download: pinned official source + safetensors, never at Renderer startup."""
import argparse
import hashlib
import json
import shutil
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CODE_REVISION = 'f8cb2dba4d08b3dfcd392cf9b2946a11f1f48c23'
MODEL_REVISION = '5def370459c15ee36c524068bb8338b3f6c6d6c0'
MODEL_ID = 'mlfarinha/pixlrelight'
VENDOR = ROOT / '.vendor' / ('pixlrelight-' + CODE_REVISION)


def fetch_source():
    url = f'https://api.github.com/repos/{MODEL_ID}/git/trees/{CODE_REVISION}?recursive=1'
    with urllib.request.urlopen(url, timeout=30) as response:
        tree = json.load(response)
    hashes = {}
    for item in tree['tree']:
        name = item['path']
        if item['type'] != 'blob' or not (name.startswith('src/') or name in ('LICENSE', 'README.md', 'requirements.txt', 'infer.py')):
            continue
        if '..' in Path(name).parts or item['size'] > 1024 * 1024:
            raise ValueError('Unexpected source manifest')
        path = VENDOR / name
        if not path.exists():
            with urllib.request.urlopen(f'https://raw.githubusercontent.com/{MODEL_ID}/{CODE_REVISION}/{name}', timeout=30) as response:
                data = response.read(1024 * 1024 + 1)
            if len(data) != item['size']:
                raise ValueError('Source size mismatch')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        data = path.read_bytes()
        if hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() != item['sha']:
            raise ValueError(f'Pinned source mismatch: {name}')
        hashes[name] = hashlib.sha256(data).hexdigest()
    (VENDOR / 'source-manifest.json').write_text(json.dumps(hashes, indent=2), encoding='utf-8')
    print(f'Pinned source verified: {VENDOR}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--weights', action='store_true', help='Explicitly fetch 2.564 GB CC BY-NC 4.0 research weights')
    args = parser.parse_args()
    fetch_source()
    if args.weights:
        if shutil.disk_usage(ROOT).free < 6 * 1024**3:
            raise RuntimeError('Need at least 6 GiB free before optional checkpoint download')
        from huggingface_hub import snapshot_download
        path = snapshot_download(MODEL_ID, revision=MODEL_REVISION,
            allow_patterns=['config.yaml', 'model.safetensors', 'README.md'],
            cache_dir=str(ROOT / '.cache' / 'huggingface'), max_workers=1)
        print(f'Pinned checkpoint downloaded: {path}', flush=True)


if __name__ == '__main__':
    main()
