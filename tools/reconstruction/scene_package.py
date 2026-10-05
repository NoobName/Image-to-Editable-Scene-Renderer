"""ScenePackage v1 file interchange API and CLI. Python 3.10+.

Legacy v1 validation is standard-library-only; optional appearance image checks use Pillow.

Public API: create_package, copy_asset, write_package, load_package, add_auxiliary.
All data exchange happens through files. Renderer is an independent consumer.
"""
from __future__ import annotations
import argparse
import json
import math
import os
from pathlib import Path
import shutil
import struct
import tempfile
from urllib.parse import unquote

try:
    from .package_schema import normalize, read_json, validate, SCHEMA
except ImportError:
    from package_schema import normalize, read_json, validate, SCHEMA

FOLDERS = ("meshes", "textures", "masks", "debug")
IMAGE_EXTENSIONS = (".png", ".jpg", ".jpeg")


def create_package(root):
    root = Path(root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    for folder in (*FOLDERS,"objects"):
        (root / folder).mkdir(exist_ok=True)
        if not (root / folder).resolve().is_relative_to(root):
            raise ValueError(f"Package directory escapes root: {folder}")
    return root


def asset_path(root, relative, folder, extensions, must_exist=True):
    if not isinstance(relative, str) or not relative.startswith(folder + "/"):
        raise ValueError(f"Expected a path below {folder}/: {relative!r}")
    if any(ord(c) < 32 or ord(c) == 127 or c in '\\:*?"<>|' for c in relative):
        raise ValueError(f"Invalid package path: {relative!r}")
    if any(part in ("", ".", "..") or part.endswith((".", " ")) for part in relative.split("/")):
        raise ValueError(f"Invalid package path: {relative!r}")
    if Path(relative).suffix.lower() not in extensions:
        raise ValueError(f"Unsupported file extension: {relative}")
    root = Path(root).resolve(strict=True)
    path = (root / relative).resolve(strict=must_exist)
    if not path.is_relative_to(root) or (must_exist and not path.is_file()):
        raise ValueError(f"Asset escapes package or is not a file: {relative}")
    return path


def _gltf_dependencies(path, root):
    """Inspect local buffer/image references only; full glTF decoding belongs to C++."""
    if path.suffix.lower() == ".glb":
        with path.open("rb") as stream:
            header = stream.read(20)
            if len(header) != 20:
                raise ValueError("Truncated GLB header")
            magic, version, total, length, kind = struct.unpack("<5I", header)
            if magic != 0x46546C67 or version != 2 or kind != 0x4E4F534A or total != path.stat().st_size or length > 4*1024*1024 or length > total-20:
                raise ValueError("Invalid GLB JSON chunk")
            data = json.loads(stream.read(length).decode("utf-8"))
    else:
        data = read_json(path)
    for value in data.get("buffers", []) + data.get("images", []):
        uri = value.get("uri")
        if uri is None or uri.startswith("data:"):
            continue
        decoded = unquote(uri, errors="strict")
        if ":" in decoded or "\0" in decoded or Path(decoded).is_absolute():
            raise ValueError("glTF dependencies must use local relative URIs")
        dependency = (path.parent / decoded).resolve(strict=True)
        if not dependency.is_relative_to(root) or not dependency.is_file():
            raise ValueError(f"glTF dependency escapes package: {uri}")


def load_region(root, obj):
    region = read_json(asset_path(root,obj["region"],"objects",(".json",)))
    validate(region,SCHEMA["$defs"]["region"])
    if region["id"]!=obj.get("id") or region["name"]!=obj["name"]:
        raise ValueError("Region id/name must match its scene object")
    asset_path(root,region["mask"],"objects",(".png",))
    x,y,w,h = region["boundingBox"]
    width,height = region["imageSize"]
    if not w or not h or x+w>width or y+h>height or region["pixelCount"]>w*h or region["validDepthPixels"]>region["pixelCount"]:
        raise ValueError("Invalid region bounds or pixel counts")
    if (region["averageDepth"] is None)!=(region["validDepthPixels"]==0):
        raise ValueError("Region averageDepth is null exactly when validDepthPixels is zero")
    if ("mesh" in obj)!=(region["triangleCount"]>0):
        raise ValueError("Region triangleCount and mesh presence disagree")
    return region


def validate_files(root, data):
    root = Path(root).resolve(strict=True)
    for folder in FOLDERS:
        directory = (root / folder).resolve(strict=True)
        if not directory.is_dir() or not directory.is_relative_to(root):
            raise ValueError(f"Invalid package directory: {folder}")
    c = data["camera"]
    delta = [b-a for a, b in zip(c["position"], c["target"])]
    distance = sum(v*v for v in delta)
    if c["far"] <= c["near"]:
        raise ValueError("Camera far must exceed near")
    if distance < 1e-8 or (delta[0]**2 + delta[2]**2) / distance < math.sin(.01)**2:
        raise ValueError("Camera position/target are degenerate or pitch exceeds camera limits")
    if "hdri" in data["environment"]:
        asset_path(root, data["environment"]["hdri"], "textures", (".hdr",))
    for light in data["lights"]:
        if light["type"] == "directional" and sum(v*v for v in light["direction"]) < 1e-8:
            raise ValueError("Directional light needs a nonzero direction")
    names, models, ids, label_ids = set(), set(), set(), set()
    for obj in data["objects"]:
        if obj["name"] in names:
            raise ValueError(f"Duplicate object name: {obj['name']}")
        names.add(obj["name"])
        identity = obj.get("id","legacy:"+obj["name"])
        if identity in ids:
            raise ValueError("Duplicate object ID")
        ids.add(identity)
        if "region" in obj:
            region = load_region(root,obj)
            if region["labelId"] in label_ids:
                raise ValueError("Duplicate region label ID")
            label_ids.add(region["labelId"])
        elif "mesh" not in obj:
            raise ValueError("Object needs a mesh or region metadata")
        if any(abs(v) <= .0001 for v in obj["transform"]["scale"]):
            raise ValueError("Object scale must have abs(component) > 0.0001")
        if "mesh" in obj:
            folder = "objects" if obj["mesh"].startswith("objects/") else "meshes"
            path = asset_path(root, obj["mesh"], folder, (".gltf", ".glb"))
            if path not in models:
                _gltf_dependencies(path, root)
                models.add(path)
        for key in ("baseColor", "normal", "roughness", "metallic", "emissive", "occlusion", "originalImage", "confidence"):
            if key in obj.get("material", {}):
                asset_path(root, obj["material"][key], "textures", IMAGE_EXTENSIONS)
    for key, value in data["auxiliary"].items():
        asset_path(root, value, "masks" if key == "segmentation" else "debug",
                   (".png",) if key == "segmentation" else (".exr",))
    try:
        from .appearance_contract import load_extension
    except ImportError:
        from appearance_contract import load_extension
    appearance = load_extension(root)  # Optional, but fail closed when present and damaged.
    if (root / "analysis/analysis.json").exists():
        try:
            from .analysis_contract import load_analysis
        except ImportError:
            from analysis_contract import load_analysis
        load_analysis(root, appearance)
    if (root / "lighting/lighting.json").exists():
        try:
            from .lighting_contract import load_lighting
        except ImportError:
            from lighting_contract import load_lighting
        load_lighting(root, appearance)


def load_package(package):
    path = Path(package)
    if path.is_dir():
        path /= "scene.json"
    if path.name != "scene.json":
        raise ValueError("Expected a package directory or scene.json")
    root = path.parent.resolve(strict=True)
    if not path.resolve(strict=True).is_relative_to(root):
        raise ValueError("Manifest escapes package")
    data = normalize(read_json(path))
    validate_files(root, data)
    return data


def copy_asset(root, source, relative):
    """Copy bytes without transcoding. Refuses to overwrite different existing bytes."""
    root = create_package(root)
    folder = relative.split("/")[0]
    if folder not in (*FOLDERS,"objects","relighting"):
        raise ValueError("Asset must be in a standard package directory")
    target = asset_path(root, relative, folder, (Path(relative).suffix.lower(),), must_exist=False)
    source = Path(source).resolve(strict=True)
    if target.exists():
        if source != target and source.read_bytes() != target.read_bytes():
            raise FileExistsError(f"Refusing to overwrite asset: {target}")
        return relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    return relative


def add_auxiliary(root, data, kind, source):
    """Register existing analysis data; no EXR/PNG decoding, generation or AI inference."""
    if kind not in ("depth", "normal", "pointmap", "segmentation"):
        raise ValueError("Unknown auxiliary kind")
    extension = ".png" if kind == "segmentation" else ".exr"
    if Path(source).suffix.lower() != extension:
        raise ValueError(f"{kind} requires {extension}")
    folder = "masks" if kind == "segmentation" else "debug"
    relative = copy_asset(root, source, f"{folder}/{kind}{extension}")
    data.setdefault("auxiliary", {})[kind] = relative
    return relative


def write_package(root, data):
    """Validate first, then atomically replace scene.json. Returns normalized data."""
    root = create_package(root)
    result = normalize(data)
    validate_files(root, result)
    write_json_atomic(root / "scene.json", result)
    return result


def write_json_atomic(path, data):
    """Publish a UTF-8 JSON snapshot beside its temporary file, on the same filesystem."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if len(encoded.encode("utf-8")) > 4*1024*1024:
        raise ValueError("Manifest exceeds 4 MiB")
    temp = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent, suffix=".tmp", delete=False) as stream:
            temp = Path(stream.name)
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    finally:
        if temp is not None:
            temp.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("example", "validate", "normalize"))
    parser.add_argument("package", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "example":
            try:
                from .package_example import create_example
            except ImportError:
                from package_example import create_example
            create_example(args.package)
            print(f"Created {args.package / 'scene.json'}")
        else:
            data = load_package(args.package)
            print(json.dumps(data, ensure_ascii=False, indent=2) if args.command == "normalize" else "ScenePackage manifest and file references are valid.")
    except (ValueError, OSError, KeyError, struct.error) as error:
        parser.exit(1, f"ScenePackage: {error}\n")


if __name__ == "__main__":
    main()
