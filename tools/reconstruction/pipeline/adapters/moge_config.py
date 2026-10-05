"""Audited official source/weight revisions; kept outside the generic pipeline."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MOGE_REVISION = "b942f00bdc2a2a23ebb474fbe034d487e6dcceec"
UTILS_REVISION = "3fab839f0be9931dac7c8488eb0e1600c236e183"
MODEL_ID = "Ruicheng/moge-2-vits-normal"
MODEL_REVISION = "26b477f41595707c5db6770294c0d1721e8ed4ed"
MODEL_SHA256 = "79a16621928c2bf0ed04659218c55c01075e950507f40bb3332fb4c873d3e1dc"
SOURCES = (
    (f"https://codeload.github.com/microsoft/MoGe/zip/{MOGE_REVISION}", f"MoGe-{MOGE_REVISION}",
     "859993ee0829cc665154416a70749c42b34fecb2184e177debe299a83e332bec"),
    (f"https://codeload.github.com/EasternJournalist/utils3d/zip/{UTILS_REVISION}", f"utils3d-{UTILS_REVISION}",
     "ad4dbdf7605c31806194374189051cf4c82a3b6bd186ec2bc9b53c81d301d520"),
)
