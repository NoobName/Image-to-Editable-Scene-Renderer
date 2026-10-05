"""Read a single JPEG/PNG, orient it once, convert to sRGB, and bound working memory."""
import hashlib
from io import BytesIO
from pathlib import Path
import warnings
import numpy as np
from PIL import Image, ImageCms, ImageOps
from .types import InputImage

MAX_INPUT_PIXELS = 40_000_000
MAX_INPUT_BYTES = 128 * 1024 * 1024
MAX_DIMENSION = 16384
MAX_WORKING_BYTES = 1536 * 1024 * 1024


def orientation_matrix(orientation, width, height):
    """Column-vector image coordinates: edges at integers, first center at (0.5,0.5)."""
    return {1: [1,0,0, 0,1,0, 0,0,1], 2: [-1,0,width, 0,1,0, 0,0,1],
            3: [-1,0,width, 0,-1,height, 0,0,1], 4: [1,0,0, 0,-1,height, 0,0,1],
            5: [0,1,0, 1,0,0, 0,0,1], 6: [0,-1,height, 1,0,0, 0,0,1],
            7: [0,-1,height, -1,0,width, 0,0,1], 8: [0,1,0, -1,0,width, 0,0,1]}[orientation]


def load_image(path: Path, max_size: int = 1024) -> InputImage:
    if not 16 <= max_size <= 2048:
        raise ValueError("max-size must be between 16 and 2048")
    path = Path(path).resolve(strict=True)
    if not path.is_file() or path.stat().st_size > MAX_INPUT_BYTES:
        raise ValueError("Input must be a file no larger than 128 MiB")
    encoded = path.read_bytes()
    try:
        rgb, canonical, source_size, normalization = _decode_image(encoded, max_size)
    except (Image.DecompressionBombWarning, Image.DecompressionBombError, ImageCms.PyCMSError) as error:
        raise ValueError(f"Cannot decode input image: {error}") from error
    rgb.setflags(write=False)
    canonical.setflags(write=False)
    return InputImage(path, rgb, source_size, hashlib.sha256(encoded).hexdigest(),
                      normalization["iccPolicy"] == "embedded-to-srgb", canonical, encoded, normalization)


def _decode_image(encoded: bytes, max_size: int):
    with warnings.catch_warnings():
        warnings.simplefilter("error", Image.DecompressionBombWarning)
        with Image.open(BytesIO(encoded)) as original:
            if original.format not in ("JPEG", "PNG") or getattr(original, "n_frames", 1) != 1:
                raise ValueError("Input must be a single-frame JPEG or PNG")
            source_size = original.size
            if original.width * original.height > MAX_INPUT_PIXELS:
                raise ValueError("Input exceeds 40 megapixels")
            if max(original.size) > MAX_DIMENSION:
                raise ValueError("Canonical image dimension exceeds 16384; anchor will not be downscaled")
            # Conservative simultaneous decode/ICC/alpha/array working-set estimate, not an RSS promise.
            if original.width * original.height * 32 + len(encoded) * 2 > MAX_WORKING_BYTES:
                raise ValueError("Canonical decode exceeds the 1536 MiB working-memory budget")
            orientation = original.getexif().get(274, 1)
            if type(orientation) is not int or not 1 <= orientation <= 8:
                raise ValueError("Invalid EXIF orientation")
            oriented = ImageOps.exif_transpose(original)
            # Preserve alpha independently when converting RGB through an ICC profile.
            alpha = oriented.convert("RGBA").getchannel("A")
            profile_bytes = oriented.info.get("icc_profile")
            if profile_bytes:
                try:
                    profile = ImageCms.ImageCmsProfile(BytesIO(profile_bytes))
                    color_source = oriented if oriented.mode in ("RGB", "RGBA", "CMYK", "LAB", "L") else oriented.convert("RGB")
                    color = ImageCms.profileToProfile(color_source, profile, ImageCms.createProfile("sRGB"), outputMode="RGB")
                except (OSError, ImageCms.PyCMSError) as error:
                    raise ValueError(f"Invalid embedded ICC profile: {error}") from error
            else:
                color = oriented.convert("RGB")
            # The v1 Dummy object is opaque; alpha is composited on white, not silently dropped.
            color = Image.composite(color, Image.new("RGB", color.size, "white"), alpha)
            canonical = np.array(color, dtype=np.uint8, copy=True)
            normalization = {"pipeline": "pillow-exif-icc-white-rgb8-v1", "exifOrientation": orientation,
                "storedToCanonical": orientation_matrix(orientation, *source_size), "inputMode": original.mode,
                "iccPolicy": "embedded-to-srgb" if profile_bytes else "assumed-srgb",
                "alphaPolicy": "white-in-srgb", "precision": "rgb8-normalized-not-high-bit-depth"}
            color.thumbnail((max_size, max_size), Image.Resampling.LANCZOS)
            if not 0.01 <= color.width / color.height <= 100:
                raise ValueError("Image aspect must be between 0.01 and 100 for ScenePackage v1")
            rgb = np.array(color, dtype=np.uint8, copy=True)
    return rgb, canonical, source_size, normalization
