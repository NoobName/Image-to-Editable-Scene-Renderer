"""Fetch pinned, hash-verified official inference sources without optional training/demo extras."""
import hashlib
from io import BytesIO
from pathlib import Path
from tempfile import TemporaryDirectory
from urllib.request import urlopen
from zipfile import ZipFile
from pipeline.adapters.moge_config import ROOT, SOURCES


def main():
    vendor = ROOT / ".vendor"
    vendor.mkdir(exist_ok=True)
    for url, directory, expected_hash in SOURCES:
        destination = vendor / directory
        if destination.is_dir():
            print(f"Using existing pinned source: {destination}")
            continue
        print(f"Downloading {url}", flush=True)
        with urlopen(url, timeout=60) as response:
            data = response.read(32*1024*1024+1)
        if hashlib.sha256(data).hexdigest() != expected_hash:
            raise RuntimeError(f"Source archive SHA256 mismatch: {directory}")
        with TemporaryDirectory(dir=vendor, prefix="source-staging-") as temporary:
            root = Path(temporary).resolve()
            with ZipFile(BytesIO(data)) as archive:
                for entry in archive.infolist():
                    target = (root / entry.filename).resolve()
                    if not target.is_relative_to(root) or (entry.external_attr >> 16) & 0o170000 == 0o120000:
                        raise ValueError("Unsafe archive path")
                archive.extractall(root)
            (root / directory).rename(destination)
        print(f"Installed official source: {directory}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
