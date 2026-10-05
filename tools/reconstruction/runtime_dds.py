"""Restricted DX10 DDS: one 2D slice/mip, tightly packed little-endian numeric texels."""
import struct
from pathlib import Path
import numpy as np

FORMATS = {"R32_FLOAT": (41, "<f4", 1), "RGBA32_FLOAT": (2, "<f4", 4), "R32_UINT": (42, "<u4", 1)}
MAX_EDGE = 2048
MAX_PIXELS = 2048 * 2048


def header(width, height, format_name):
    code, _, channels = FORMATS[format_name]
    if not (1 <= width <= MAX_EDGE and 1 <= height <= MAX_EDGE and width * height <= MAX_PIXELS):
        raise ValueError("Runtime DDS dimensions exceed 2048x2048")
    words = [124, 0x100F, height, width, width * channels * 4, 0, 1] + [0] * 11
    words += [32, 4, 0x30315844, 0, 0, 0, 0, 0, 0x1000, 0, 0, 0, 0]
    return b"DDS " + struct.pack("<31I", *words) + struct.pack("<5I", code, 3, 0, 1, 0)


def write_dds(path, pixels, format_name):
    _, dtype, channels = FORMATS[format_name]
    expected = np.dtype(dtype)
    if pixels.dtype != expected or pixels.ndim != (2 if channels == 1 else 3) or (channels == 4 and pixels.shape[2] != 4):
        raise ValueError("Runtime DDS dtype/channels mismatch")
    if not np.isfinite(pixels).all():
        raise ValueError("Runtime DDS rejects NaN/Inf, including invalid texels")
    h, w = pixels.shape[:2]
    Path(path).write_bytes(header(w, h, format_name) + np.ascontiguousarray(pixels).tobytes())


def read_dds(path):
    path = Path(path)
    if path.stat().st_size > 148 + MAX_PIXELS * 16:
        raise ValueError("Runtime DDS byte limit exceeded")
    data = path.read_bytes()
    if len(data) < 148 or data[:4] != b"DDS ":
        raise ValueError("Invalid/truncated runtime DDS header")
    code = struct.unpack_from("<I", data, 128)[0]
    format_name = next((name for name, info in FORMATS.items() if info[0] == code), None)
    if format_name is None:
        raise ValueError("Unsupported runtime DDS DXGI format")
    h, w = struct.unpack_from("<2I", data, 12)
    if data[:148] != header(w, h, format_name):
        raise ValueError("Runtime DDS requires exact restricted DX10 header (2D, one mip/slice, uncompressed)")
    _, dtype, channels = FORMATS[format_name]
    if len(data) != 148 + w * h * channels * 4:
        raise ValueError("Runtime DDS exact payload byte count mismatch")
    pixels = np.frombuffer(data, dtype=dtype, offset=148).reshape((h, w) if channels == 1 else (h, w, 4)).copy()
    if not np.isfinite(pixels).all():
        raise ValueError("Runtime DDS contains NaN/Inf")
    return format_name, pixels
