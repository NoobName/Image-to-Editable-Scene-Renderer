"""Pinned official SAM 2.1 Tiny assets; no model framework imported here."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE_REVISION = "2b90b9f5ceec907a1c18123530e92e794ad901a4"
SOURCE_SHA256 = "fe93082a71a885a427894b1eab76341768781b6b58a298a0717e03862097d137"
SOURCE_DIR = ROOT / ".vendor" / f"sam2-{SOURCE_REVISION}"
MODEL_ID = "facebook/sam2.1-hiera-tiny"
MODEL_REVISION = "de431c4043854a71d8101e17995dfe596bf101a5"
MODEL_FILE = "sam2.1_hiera_tiny.pt"
MODEL_SHA256 = "7402e0d864fa82708a20fbd15bc84245c2f26dff0eb43a4b5b93452deb34be69"
MODEL_CONFIG = "configs/sam2.1/sam2.1_hiera_t.yaml"
