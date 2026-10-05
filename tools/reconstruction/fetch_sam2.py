"""Fetch pinned official SAM 2 sources and Tiny weights into project-local ignored caches."""
import hashlib
import shutil
from pathlib import Path
from tempfile import TemporaryDirectory
from urllib.request import urlopen
from zipfile import ZipFile
from pipeline.adapters.sam2_config import (ROOT,SOURCE_REVISION,SOURCE_SHA256,SOURCE_DIR,
                                           MODEL_ID,MODEL_REVISION,MODEL_FILE,MODEL_SHA256)


def main():
    cache = ROOT/".cache/sam2"
    cache.mkdir(parents=True,exist_ok=True)
    archive = cache/"source.zip"
    if not archive.exists():
        with urlopen(f"https://codeload.github.com/facebookresearch/sam2/zip/{SOURCE_REVISION}",timeout=60) as response:
            content = response.read(128*1024*1024+1)
        if len(content)>128*1024*1024:
            raise RuntimeError("SAM 2 source archive exceeds limit")
        archive.write_bytes(content)
    with archive.open("rb") as stream:
        if hashlib.file_digest(stream,"sha256").hexdigest()!=SOURCE_SHA256:
            raise RuntimeError("SAM 2 source SHA256 mismatch")
    if not SOURCE_DIR.exists():
        SOURCE_DIR.parent.mkdir(exist_ok=True)
        with TemporaryDirectory(dir=SOURCE_DIR.parent,prefix="sam2-staging-") as temp:
            root = Path(temp).resolve()
            with ZipFile(archive) as source:
                if sum(entry.file_size for entry in source.infolist())>512*1024*1024:
                    raise ValueError("SAM 2 source exceeds extraction limit")
                links = []
                for entry in source.infolist():
                    target = (root/entry.filename).resolve()
                    if not target.is_relative_to(root):
                        raise ValueError("Unsafe archive entry")
                    if (entry.external_attr>>16)&0o170000==0o120000:
                        referent = (target.parent/source.read(entry).decode("utf-8")).resolve()
                        if not referent.is_relative_to(root):
                            raise ValueError("Archive link escapes source tree")
                        links.append((target,referent))
                    else:
                        source.extract(entry,root)
                # Official repo has four YAML aliases. Materialize local files on Windows,
                # never create symlinks or require administrator/developer-mode privileges.
                for target,referent in links:
                    if not referent.is_file():
                        raise ValueError("Archive alias is not a regular extracted file")
                    shutil.copyfile(referent,target)
            (root/SOURCE_DIR.name).rename(SOURCE_DIR)
    from huggingface_hub import hf_hub_download
    weights = Path(hf_hub_download(MODEL_ID,MODEL_FILE,revision=MODEL_REVISION,cache_dir=str(ROOT/".cache/huggingface")))
    with weights.open("rb") as stream:
        if hashlib.file_digest(stream,"sha256").hexdigest()!=MODEL_SHA256:
            raise RuntimeError("SAM 2 weights SHA256 mismatch")
    print(f"SAM 2 source: {SOURCE_DIR}\nWeights verified: {weights}")


if __name__ == "__main__":
    main()
