"""Check the Prompt 12 fixed-camera GPU smoke captures against the exported Chair mask."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--captures", type=Path, default=Path("generated"))
    parser.add_argument("--object-id", default="chair")
    args = parser.parse_args()
    manifest = json.loads((args.package / "scene.json").read_text(encoding="utf-8"))
    camera = manifest["camera"]
    if camera["position"] != [0, 0, 0] or camera["target"][:2] != [0, 0]:
        raise ValueError("This comparison requires the exported original camera at the origin")
    item = next(item for item in manifest["objects"] if item["id"] == args.object_id)
    region = json.loads((args.package / item["region"]).read_text(encoding="utf-8"))
    mask = Image.open(args.package / region["mask"]).convert("L")
    # Allow two source pixels around the region contour for projected pixel-center rounding.
    mask = np.asarray(mask.filter(ImageFilter.MaxFilter(5))) > 0
    results = {}
    pairs = (("hide", "base35", "hide35"), ("restore", "base65", "restore65"),
             ("base_color", "base90", "edit90"), ("roughness", "rough-base", "rough-edit"))
    for name, before, after in pairs:
        a, b = [np.asarray(Image.open(args.captures / f"prompt12-{suffix}.bmp").convert("RGB"))
                for suffix in (before, after)]
        if a.shape != b.shape:
            raise ValueError(f"Capture sizes differ: {name}")
        changed = np.any(a != b, axis=2)
        h, w = changed.shape
        photo_width = camera["aspect"] * h
        yy, xx = np.indices((h, w))
        source_x = np.floor((xx + .5 - (w - photo_width) / 2) / photo_width * mask.shape[1]).astype(int)
        source_y = np.floor((yy + .5) / h * mask.shape[0]).astype(int)
        in_bounds = (source_x >= 0) & (source_x < mask.shape[1]) & (source_y < mask.shape[0])
        projected_mask = in_bounds & mask[np.clip(source_y, 0, mask.shape[0]-1), np.clip(source_x, 0, mask.shape[1]-1)]
        count, outside = int(changed.sum()), int((changed & ~projected_mask).sum())
        if name == "restore":
            if count:
                raise AssertionError("Restored object must reproduce the original GPU image exactly")
        elif count < 100 or outside:
            raise AssertionError(f"{name}: changed={count}, outside selected region={outside}")
        results[name] = {"changed_pixels": count, "outside_region_with_2px_margin": outside,
                         "size": [w, h]}
    names = [suffix for _, a, b in pairs for suffix in (a, b)] + ["indoor-ui", "traffic-ui", "release", "warp"]
    for suffix in names:
        log = (args.captures / f"prompt12-{suffix}.log").read_text(encoding="utf-8-sig")
        if "nonfinite=0" not in log or "Completed frames=" not in log:
            raise AssertionError(f"Missing finite HDR / completed frame evidence: {suffix}")
        if suffix != "release" and "Validation summary: errors=0 warnings=0" not in log:
            raise AssertionError(f"Validation error or warning: {suffix}")
    results["window_runs"] = len(names)
    print(json.dumps(results, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
