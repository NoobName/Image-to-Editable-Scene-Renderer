"""Verify Prompt 15 runtime edits against GPU readbacks, including per-object isolation."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--object-id", default="chair")
    parser.add_argument("--captures", type=Path, default=Path("generated"))
    args = parser.parse_args()
    root = args.captures
    results = {}
    names = set()

    def pixels(name):
        names.add(name)
        return np.asarray(Image.open(root / f"prompt15-{name}.bmp").convert("RGB"))

    def difference(a, b):
        if a.shape != b.shape:
            raise AssertionError("Capture dimensions differ")
        return np.any(a != b, axis=2)

    base = pixels("baseline")
    for name in ("move", "rotate", "scale", "material", "sun", "light", "environment", "exposure"):
        count = int(difference(base, pixels(name)).sum())
        if count < 100:
            raise AssertionError(f"{name} did not visibly change the GPU output: {count}")
        results[name] = {"changed_pixels": count}
    for name in ("restore-material", "restore-transform"):
        count = int(difference(base, pixels(name)).sum())
        if count:
            raise AssertionError(f"{name} did not restore the exact original image: {count}")
        results[name] = {"changed_pixels": count}

    manifest = json.loads((args.package / "scene.json").read_text(encoding="utf-8"))
    camera = manifest["camera"]
    if camera["position"] != [0, 0, 0] or camera["target"][:2] != [0, 0]:
        raise ValueError("Mask projection requires the original camera at the origin")
    obj = next(obj for obj in manifest["objects"] if obj["id"] == args.object_id)
    region = json.loads((args.package / obj["region"]).read_text(encoding="utf-8"))
    mask = np.asarray(Image.open(args.package / region["mask"]).convert("L").filter(ImageFilter.MaxFilter(5))) > 0
    for name in ("albedo", "roughness", "metallic"):
        a, b = pixels(f"{name}-base"), pixels(f"{name}-edit")
        changed = difference(a, b)
        h, w = changed.shape
        image_width = camera["aspect"] * h
        yy, xx = np.indices((h, w))
        sx = np.floor((xx + .5 - (w - image_width) / 2) / image_width * mask.shape[1]).astype(int)
        sy = np.floor((yy + .5) / h * mask.shape[0]).astype(int)
        valid = (sx >= 0) & (sx < mask.shape[1]) & (sy >= 0) & (sy < mask.shape[0])
        projected = valid & mask[np.clip(sy, 0, mask.shape[0] - 1), np.clip(sx, 0, mask.shape[1] - 1)]
        count, outside = int(changed.sum()), int((changed & ~projected).sum())
        if count < 100 or outside:
            raise AssertionError(f"{name} isolation failed: changed={count}, outside={outside}")
        results[name] = {"changed_pixels": count, "outside_selected_region_2px_margin": outside}
        if name in ("roughness", "metallic"):
            expected = 255 * (.18 if name == "roughness" else .9)
            error = float(np.abs(b[changed].astype(float) - expected).max())
            if error > 1.01:
                raise AssertionError(f"{name} override is not the requested constant: max error={error}")
            results[name]["constant_max_error_8bit"] = error

    count = int(difference(pixels("estimated-roughness-base"), pixels("estimated-roughness-edit")).sum())
    if count:
        raise AssertionError("Material override modified the raw Estimated Roughness view")
    results["raw_estimated_roughness"] = {"changed_pixels": count}
    count = int(difference(pixels("lab-baseline"), pixels("lab-normal")).sum())
    if count < 100:
        raise AssertionError("Normal strength did not affect the authored normal map")
    results["normal_strength"] = {"changed_pixels": count}
    names.update(("ui", "release", "warp"))
    for name in names:
        log = (root / f"prompt15-{name}.log").read_text(encoding="utf-8-sig")
        if "nonfinite=0" not in log or "Completed frames=90" not in log:
            raise AssertionError(f"Missing finite HDR/completion evidence: {name}")
        if name != "release" and "Validation summary: errors=0 warnings=0" not in log:
            raise AssertionError(f"DX12 validation failed: {name}")
    results["window_runs"] = len(names)
    output = json.dumps(results, indent=2)
    (root / "prompt15-pixel-results.json").write_text(output + "\n", encoding="utf-8")
    print(output)


if __name__ == "__main__":
    main()
