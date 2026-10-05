"""Verify Prompt 17 GPU captures and immutable source-session diagnostics (no model required)."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, default=Path("generated/prompt16"))
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[2]
    generated = project / "generated"
    fixtures = args.fixtures.resolve()
    report = {"pixelChecks": {}, "gpu": {}, "sessionChecks": {}}

    def pixels(path):
        return np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)

    def capture(name):
        return pixels(generated / f"prompt17-{name}.bmp")

    def view(name):
        data = json.loads((generated / f"prompt17-{name}.view.json").read_text(encoding="utf-8"))
        # Re-encoding with this flag rejects any nested NaN or infinity, including camera/mapping.
        json.dumps(data, allow_nan=False)
        return data

    def compare(label, actual, expected, tolerance=0):
        assert actual.shape == expected.shape, (label, actual.shape, expected.shape)
        diff = np.abs(actual - expected)
        value = {"maxLsb": int(diff.max()), "changedPixels": int(np.count_nonzero(np.any(diff, axis=2))),
                 "size": [actual.shape[1], actual.shape[0]], "toleranceLsb": tolerance}
        report["pixelChecks"][label] = value
        assert value["maxLsb"] <= tolerance, (label, value)

    source = pixels(fixtures / "analysis512/textures/source_anchor.png")
    legacy = pixels(fixtures / "legacy-upgraded/textures/source_anchor.png")
    compare("oldFinal", capture("after"), capture("before"))
    for name in ("native", "native-release", "native-warp"):
        compare(name, capture(name), source, 1)
        assert view(name)["imageRect"] == [0, 0, 1500, 1000]
    base = view("native")
    for name in ("input", "cycle", "look", "transaction"):
        compare(name, capture(name), capture("native"))
        data = view(name)
        for key in ("sourceId", "sourceSha256", "sourceCamera", "analysisMapping", "revision"):
            assert data[key] == base[key], (name, key)
    assert view("input")["freeCamera"] == base["freeCamera"]
    assert view("cycle")["freeCamera"]["position"] != base["freeCamera"]["position"]
    report["sessionChecks"]["freeCameraBefore"] = base["freeCamera"]
    report["sessionChecks"]["freeCameraAfter3DEdit"] = view("cycle")["freeCamera"]
    for name, expected_rect in (("wide", [450, 0, 900, 600]), ("narrow", [0, 300, 600, 400])):
        data = view(name)
        assert data["imageRect"] == expected_rect, (name, data)
        rgb = capture(name)
        yy, xx = np.indices(rgb.shape[:2])
        x, y, w, h = expected_rect
        outside = (xx + .5 < x) | (xx + .5 >= x + w) | (yy + .5 < y) | (yy + .5 >= y + h)
        assert np.all(rgb[outside] == [6, 8, 10]), name
        report["sessionChecks"][name] = {"imageRect": expected_rect, "letterboxRgb8": [6, 8, 10], "outsidePixels": int(outside.sum())}
    compare("legacyFallback", capture("legacy"), capture("after"))
    assert view("legacy")["workMode"] == "scene" and "sourceId" not in view("legacy")
    compare("legacyAnchor", capture("legacy-anchor"), legacy, 1)
    assert not view("legacy-anchor")["capabilities"]["fullResolutionAnchor"]
    # Fixed current three-panel layout: left=220, window padding=8; viewport begins below mode rows.
    assert view("native-ui")["viewportSize"] == [512, 341]
    compare("nativeImGui", capture("native-ui")[92:433, 228:740], legacy, 1)
    for name in ("failed", "cancelled"):
        compare(name, capture(name), capture("fit"))
        data = view(name)
        for key in ("sourceId", "sourceSha256", "sourceCamera", "analysisMapping", "revision"):
            assert data[key] == base[key], (name, key)
    reload = view("reload")
    final_extension = json.loads((Path(reload["packageRoot"]) / "relighting/relighting.json").read_text(encoding="utf-8"))
    assert reload["revision"] == 3 and reload["sourceSize"] == [1000, 1500]
    assert reload["sourceId"] != base["sourceId"]
    assert reload["sourceSha256"] == final_extension["sourceImage"]["sha256"]
    assert reload["sourceCamera"] == final_extension["sourceCamera"]
    report["sessionChecks"]["reload"] = reload
    names = ("after", "fit", "native", "native-release", "native-warp", "input", "cycle", "look", "wide", "narrow",
             "grid-ui", "legacy", "legacy-anchor", "native-ui", "exif", "transaction", "failed", "cancelled", "reload")
    for name in names:
        log = (generated / f"prompt17-{name}.log").read_text(encoding="utf-8")
        assert "Completed frames=" in log, name
        if name != "native-release":
            assert "Validation summary: errors=0 warnings=0" in log, name
        else:
            assert "D3D12 Debug Layer: disabled (Release)" in log, name
        if view(name)["workMode"] == "image":
            assert "Source image output: RGBA8 UNORM" in log, name
        else:
            assert "nonfinite=0" in log, name
        if name == "transaction":
            assert "failed GPU preparation retained scene/source/revision" in log
            assert "cancelled completed upload retained scene/source/revision" in log
        if name == "reload":
            assert log.count("Reconstruction Ready:") == 2 and log.count("Source session committed:") == 2
        if name == "cycle":
            assert log.count("Image mode cycle at frame=") == 14
        report["gpu"][name] = {"expectedApplicationExit": 2 if name in ("failed", "cancelled") else 0,
            "events": [line for line in log.splitlines() if any(key in line for key in (
                "Validation summary:", "Completed frames=", "Resize:", "Viewport resize:", "Reconstruction Ready:",
                "Source session committed:", "Image transaction:", "HDR statistics:", "Source image output:", "Reconstruction Error:", "cancel"))]}
    path = generated / "prompt17-results.json"
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(f"PASS: {len(names)} GPU runs, {len(report['pixelChecks'])} pixel comparisons; source/camera/mapping immutable. {path}")
    for key, value in report["pixelChecks"].items():
        print(key, value)


if __name__ == "__main__":
    main()
