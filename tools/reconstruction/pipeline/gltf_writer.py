"""Write one self-contained glTF 2.0 GLB mesh. No third-party scene/export dependency."""
import json
from pathlib import Path
import struct
import numpy as np
from .types import GeometryResult


def write_glb(path: Path, geometry: GeometryResult, base_color_png: bytes | None = None,
              *, metallic: float = 0, roughness: float = .8, base_color_uri: str | None = None,
              name: str = "Reconstructed image surface", metallic_roughness_image: bytes | str | None = None,
              normal_image: bytes | str | None = None, normal_strength: float = 1,
              material_name: str = "Input photograph") -> None:
    # ScenePackage uses LH, glTF uses RH. The C++ importer performs the inverse.
    positions = geometry.positions.copy()
    normals = geometry.normals.copy()
    positions[:, 2] *= -1
    normals[:, 2] *= -1
    indices = geometry.indices[:, [0, 2, 1]]
    binary = bytearray()
    views, accessors = [], []

    def attribute(values, kind, component_type=5126, target=34962, bounds=False):
        array = np.ascontiguousarray(values, dtype="<f4" if component_type == 5126 else "<u4")
        while len(binary) % 4:
            binary.append(0)
        offset = len(binary)
        payload = array.tobytes()
        binary.extend(payload)
        view = len(views)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload), "target": target})
        accessor = {"bufferView": view, "componentType": component_type, "count": len(array), "type": kind}
        if bounds:
            accessor.update(min=array.min(axis=0).tolist(), max=array.max(axis=0).tolist())
        accessors.append(accessor)
        return len(accessors)-1

    position = attribute(positions, "VEC3", bounds=True)
    normal = attribute(normals, "VEC3")
    uv = attribute(geometry.texcoords, "VEC2")
    index = attribute(indices.reshape(-1), "SCALAR", component_type=5125, target=34963)
    data = {
        "asset": {"version": "2.0", "generator": "ImageSceneRenderer Grid Reconstruction"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": position, "NORMAL": normal, "TEXCOORD_0": uv},
                                    "indices": index, "material": 0, "mode": 4}]}],
        "materials": [{"name": material_name, "pbrMetallicRoughness": {"metallicFactor": metallic, "roughnessFactor": roughness}}],
        "buffers": [{"byteLength": len(binary)}], "bufferViews": views, "accessors": accessors}
    if base_color_png is not None and base_color_uri is not None:
        raise ValueError("Specify embedded PNG or external image URI, not both")
    if base_color_png is not None:
        binary += b"\0" * (-len(binary) % 4)
        image_view = len(views)
        views.append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(base_color_png)})
        binary.extend(base_color_png)
        data["images"] = [{"bufferView": image_view, "mimeType": "image/png"}]
        data["buffers"][0]["byteLength"] = len(binary)
    elif base_color_uri is not None:
        data["images"] = [{"uri":base_color_uri}]
    if "images" in data:
        data["samplers"] = [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}]
        data["textures"] = [{"source": 0, "sampler": 0}]
        data["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
    for role, image in (("metallicRoughnessTexture", metallic_roughness_image), ("normalTexture", normal_image)):
        if image is None:
            continue
        data.setdefault("images", [])
        data.setdefault("textures", [])
        data.setdefault("samplers", [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}])
        image_index = len(data["images"])
        if isinstance(image, bytes):
            binary += b"\0" * (-len(binary) % 4)
            views.append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(image)})
            binary.extend(image)
            data["images"].append({"bufferView": len(views)-1, "mimeType": "image/png"})
        else:
            data["images"].append({"uri": image})
        data["textures"].append({"source": image_index, "sampler": 0})
        binding = {"index": len(data["textures"])-1}
        if role == "normalTexture":
            binding["scale"] = normal_strength
            data["materials"][0][role] = binding
        else:
            data["materials"][0]["pbrMetallicRoughness"][role] = binding
    data["buffers"][0]["byteLength"] = len(binary)
    encoded = json.dumps(data, separators=(",", ":"), allow_nan=False).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    binary += b"\0" * (-len(binary) % 4)
    total = 12 + 8 + len(encoded) + 8 + len(binary)
    with Path(path).open("wb") as stream:
        stream.write(struct.pack("<III", 0x46546C67, 2, total))
        stream.write(struct.pack("<II", len(encoded), 0x4E4F534A))
        stream.write(encoded)
        stream.write(struct.pack("<II", len(binary), 0x004E4942))
        stream.write(binary)
