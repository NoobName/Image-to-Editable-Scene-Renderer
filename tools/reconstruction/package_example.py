"""Create a reproducible interchange sample from our own existing test assets."""
from pathlib import Path
import struct
import zlib
try:
    from .scene_package import create_package, copy_asset, write_package
except ImportError:
    from scene_package import create_package, copy_asset, write_package


def _png(path, rgba):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", 1, 1, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(b"\0" + bytes(rgba))) + chunk(b"IEND", b""))


def create_example(destination):
    destination = Path(destination)
    if destination.exists() and any(destination.iterdir()):
        raise FileExistsError("Example destination must be empty; choose a new directory")
    root = create_package(destination)
    assets = Path(__file__).resolve().parents[2] / "assets"
    copy_asset(root, assets / "models/MaterialLab/MaterialLab.glb", "meshes/MaterialLab.glb")
    copy_asset(root, assets / "models/MaterialLab/normal.png", "textures/normal.png")
    copy_asset(root, assets / "environments/SoftStudio.hdr", "textures/SoftStudio.hdr")
    _png(root / "textures/roughness.png", (90, 90, 90, 255))
    _png(root / "textures/metallic.png", (220, 220, 220, 255))
    _png(root / "textures/baseColor.png", (230, 145, 65, 255))
    # Demonstrates an actual categorical PNG. EXR data is optional and copied via add_auxiliary.
    _png(root / "masks/segmentation.png", (1, 0, 0, 255))
    for folder in ("debug",):
        (root / folder / ".gitkeep").touch()
    return write_package(root, {
        "version": 1, "coordinateSystem": "left-handed-y-up", "units": "meters",
        "camera": {"position": [8, 5, -13], "target": [0, .6, 0], "far": 200},
        "environment": {"hdri": "textures/SoftStudio.hdr", "intensity": .7, "rotationDegrees": 25},
        "lights": [{"type": "directional", "direction": [.4, -.8, .5], "intensity": 2},
                   {"type": "point", "position": [0, 4, -3], "color": [.6, .8, 1], "intensity": 12, "range": 15}],
        "objects": [{"name": "Original Materials", "mesh": "meshes/MaterialLab.glb", "transform": {"position": [-2.7, 0, 0]}},
                    {"name": "Package Material", "mesh": "meshes/MaterialLab.glb", "transform": {"position": [2.7, 0, 0], "rotationDegrees": [0, 15, 0]},
                     "material": {"baseColor": "textures/baseColor.png", "normal": "textures/normal.png",
                                  "roughness": "textures/roughness.png", "metallic": "textures/metallic.png", "normalStrength": .6}}],
        "look": {"exposure": .3, "bloom": True, "bloom-intensity": .12, "vignette": .15},
        "auxiliary": {"segmentation": "masks/segmentation.png"}})
