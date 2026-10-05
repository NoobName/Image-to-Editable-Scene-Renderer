"""Export/restoration of linear material data and display/texture encodings, without models."""
import json
from pathlib import Path
import numpy as np
from PIL import Image
from .material_estimation_backend import MaterialEstimate, validate_material, quantize


def write_material_assets(root, image, material):
    """Return the ScenePackage override; photo and intrinsic reflectance always stay separate."""
    validate_material(image, material)
    textures = root / "textures"
    Image.fromarray(image.rgb).save(textures / "original_image.png")
    Image.fromarray(material.base_color).save(textures / "object_albedo.png")
    Image.fromarray(quantize(material.normal * .5 + .5)).save(textures / "object_normal.png")
    Image.fromarray(quantize(material.roughness)).save(textures / "object_roughness.png")
    Image.fromarray(quantize(material.metallic)).save(textures / "object_metallic.png")
    Image.fromarray(quantize(material.confidence)).save(textures / "object_confidence.png")
    # glTF uses G=roughness, B=metallic; the individual data PNGs still use R.
    packed = np.stack((np.ones_like(material.roughness), material.roughness, material.metallic), axis=2)
    Image.fromarray(quantize(packed)).save(textures / "object_metallic_roughness.png")
    np.savez_compressed(root / "debug/material.npz", **{name: getattr(material, name)
        for name in ("albedo", "roughness", "metallic", "normal", "confidence")})
    (root / "debug/material_estimation.json").write_text(
        json.dumps(material.metadata, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return {"baseColor": "textures/object_albedo.png", "normal": "textures/object_normal.png",
        "roughness": "textures/object_roughness.png", "metallic": "textures/object_metallic.png",
        "originalImage": "textures/original_image.png", "confidence": "textures/object_confidence.png",
        "albedoSource": material.metadata["albedo_source"], "normalSource": material.metadata["normal_source"],
        "normalStrength": 0 if material.metadata["normal_source"] == "flat-tangent-fallback" else 1,
        "metallicFactor": 1, "roughnessFactor": 1}


def load_saved_material(package, image):
    root = Path(package)
    data, metadata = root / "debug/material.npz", root / "debug/material_estimation.json"
    if not data.exists() and not metadata.exists():
        from .types import MaterialPrediction
        return MaterialPrediction(image.rgb.copy())  # Explicit legacy placeholder; no claimed estimate.
    with np.load(data, allow_pickle=False) as arrays:
        maps = {name: arrays[name].copy() for name in ("albedo", "roughness", "metallic", "normal", "confidence")}
    result = MaterialEstimate(**maps, metadata=json.loads(metadata.read_text(encoding="utf-8")))
    validate_material(image, result)
    return result


def material_glb_options(root, material, embedded):
    if not isinstance(material, MaterialEstimate):
        return {"metallic": material.metallic, "roughness": material.roughness}
    def texture(name):
        relative = f"textures/{name}"
        return (root / relative).read_bytes() if embedded else "../../" + relative
    return {"metallic": 1, "roughness": 1,
            "metallic_roughness_image": texture("object_metallic_roughness.png"),
            "normal_image": texture("object_normal.png"),
            "normal_strength": 0 if material.metadata["normal_source"] == "flat-tangent-fallback" else 1,
            "material_name": "Intrinsic estimate" if material.metadata["albedo_source"] == "intrinsic" else "Neutral material fallback"}
