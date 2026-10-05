"""Analytic texture fixture and GPU readback checks, separate from real model inference."""
import argparse
from dataclasses import replace
import json
from pathlib import Path
import numpy as np
from PIL import Image
from pipeline.material_estimation_backend import NeutralMaterialBackend, srgb_to_linear
from pipeline.runner import ReconstructionPipeline


def make_fixture(output):
    output.parent.mkdir(parents=True, exist_ok=True)
    source = output.parent / (output.name + "-input.png")
    Image.fromarray(np.broadcast_to(np.array([26, 77, 153], np.uint8), (48, 64, 3)).copy()).save(source)
    class AnalyticMaterial(NeutralMaterialBackend):
        name = "analytic-material-validation"
        def predict(self, image):
            value = super().predict(image)
            albedo = np.broadcast_to(srgb_to_linear(np.array([128, 64, 191], np.float32) / 255), (48, 64, 3)).copy()
            return replace(value, albedo=albedo, roughness=np.full((48, 64), .25, np.float32),
                metallic=np.full((48, 64), .75, np.float32), metadata={**value.metadata,
                "backend": self.name, "synthetic_validation_fixture": True,
                "albedo_source": "intrinsic", "roughness_source": "analytic-known-value",
                "metallic_source": "analytic-known-value"})
    ReconstructionPipeline(material_backend=AnalyticMaterial()).run(source, output)


def check(directory, package):
    def capture(name):
        return np.asarray(Image.open(directory / ("prompt13-" + name + ".bmp")).convert("RGB"))
    results = {}
    for name, expected in (("probe-original", [26, 77, 153]), ("probe-albedo", [128, 64, 191]),
                           ("probe-roughness", [64, 64, 64]), ("probe-metallic", [191, 191, 191]),
                           ("probe-normal", [128, 128, 255])):
        image = capture(name)
        h, w = image.shape[:2]
        patch = image[h//2-10:h//2+10, w//2-10:w//2+10].astype(int)
        error = int(np.max(np.abs(patch - np.array(expected))))
        if error > 1:
            raise AssertionError(f"GPU {name} wrong channels/gamma: max error {error}, expected {expected}")
        results[name] = {"expected_rgb": expected, "max_error_u8": error}
    for name, base in (("original-edit", "original"), ("albedo-edit", "albedo"),
                       ("roughness-edit", "roughness"), ("original-look", "original")):
        a, b = capture(name), capture(base)
        if a.shape != b.shape or np.any(a != b):
            raise AssertionError(f"Raw material debug view contaminated by factors/post processing: {name}")
        results[name] = {"changed_pixels": 0}
    a, b = capture("original"), capture("albedo")
    difference = np.abs(a.astype(float) - b.astype(float))
    if (difference > 3).sum() < 10000:
        raise AssertionError("Real intrinsic albedo unexpectedly duplicates the illuminated input")
    results["real_albedo_vs_original"] = {"mean_absolute_difference_u8": float(difference.mean()),
        "changed_pixels_above_3": int(np.any(difference > 3, axis=2).sum())}
    with np.load(package / "debug/material.npz", allow_pickle=False) as data:
        for key in ("albedo", "roughness", "metallic", "normal", "confidence"):
            if not np.isfinite(data[key]).all():
                raise AssertionError(f"Nonfinite material: {key}")
            results[key] = {"min": float(data[key].min()), "max": float(data[key].max()), "mean": float(data[key].mean())}
    # Compare the previous stage on unchanged geometry/material with the new renderer.
    legacy = directory / "prompt12-base90.bmp"
    if legacy.exists() and (directory / "prompt13-legacy.bmp").exists():
        same = np.array_equal(np.asarray(Image.open(legacy).convert("RGB")), capture("legacy"))
        if not same:
            raise AssertionError("Legacy stage-12 Albedo pixels changed")
        results["legacy_stage12_equal"] = True
    for old, new in (("lighting-studio.bmp", "ibl-regression"), ("lighting-pcf3.bmp", "shadow-regression")):
        previous = directory / old
        if previous.exists() and (directory / ("prompt13-" + new + ".bmp")).exists():
            before = np.asarray(Image.open(previous).convert("RGB"))
            after = capture(new)
            delta = np.abs(before.astype(int) - after.astype(int))
            if before.shape != after.shape or delta.max() > 1:
                raise AssertionError(f"Lighting binding regression: {new}, max u8 delta={delta.max()}")
            results[new] = {"max_error_u8": int(delta.max())}
    logs = list(directory.glob("prompt13-*.log"))
    window_count = 0
    for path in logs:
        text = path.read_text(encoding="utf-8-sig")
        if path.name.endswith("-driver.log") or "Saved GPU frame capture:" not in text:
            continue
        if "nonfinite=0" not in text or "Completed frames=" not in text:
            raise AssertionError(f"Incomplete or nonfinite GPU run: {path}")
        if path.stem != "prompt13-release" and "Validation summary: errors=0 warnings=0" not in text:
            raise AssertionError(f"Debug validation failure: {path}")
        window_count += 1
    results["window_runs"] = window_count
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--make-fixture", type=Path)
    parser.add_argument("--captures", type=Path, default=Path("generated"))
    parser.add_argument("--package", type=Path, default=Path("generated/scene13"))
    args = parser.parse_args()
    if args.make_fixture:
        make_fixture(args.make_fixture)
    else:
        check(args.captures, args.package)
