"""Verify actual Prompt 18 window captures, raw GPU readbacks and preserved saved-model evidence."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image
from runtime_dds import read_dds


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--real", type=Path, default=Path("generated/scene18"))
    args = parser.parse_args()
    generated = Path("generated")
    report = {"pixels": {}, "windows": {}, "numeric": {}, "preserved": []}
    views = ("original", "grid", "depth", "geometry-normal", "world-normal", "position", "validity", "region", "albedo", "roughness", "metallic", "geometry-confidence", "material-confidence", "region-confidence", "tangent-normal")
    names = ["after"] + ["real-"+v for v in views] + ["ui", "warp-resize", "views", "cycle", "source-cycle", "transaction", "release", "wide", "narrow", "unavailable", "highres"]
    names += ["step-"+v for v in ("original", "depth", "geometry-normal", "position", "validity", "region")]
    names += ["step-native", "failed", "cancelled", "reload"]

    def read(path):
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        json.dumps(data, allow_nan=False)
        return data

    def info(name):
        return read(generated/f"prompt18-{name}.view.json")

    def pixels(path):
        return np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)

    def capture(name):
        return pixels(generated/f"prompt18-{name}.bmp")

    def compare(name, actual, expected, tolerance=0):
        assert actual.shape == expected.shape, (name, actual.shape, expected.shape)
        diff = np.abs(actual-expected)
        report["pixels"][name] = {"maxLSB": int(diff.max()), "changedPixels": int(np.any(diff, -1).sum())}
        assert diff.max() <= tolerance, (name, report["pixels"][name])

    compare("oldFinal", capture("after"), capture("before"))
    compare("sourceNative", capture("real-original"), pixels(args.real/"textures/source_anchor.png"), 1)
    compare("highres", capture("highres"), pixels(generated/"prompt16/analysis512/textures/source_anchor.png"), 1)
    compare("sourceAfter3DEdits", capture("source-cycle"), capture("real-original"))
    compare("positionAfter3DEdits", capture("cycle"), capture("real-position"))
    compare("sourceAcrossGeometryHoles", capture("step-native"), pixels(generated/"prompt18/step/textures/source_anchor.png"), 1)
    compare("failedPreservesPosition", capture("failed"), capture("views"))
    compare("cancelledPreservesPosition", capture("cancelled"), capture("views"))
    compare("transactionPreservesPosition", capture("transaction"), capture("views"))
    base = info("real-original")
    for name in ("cycle", "source-cycle", "transaction", "failed", "cancelled"):
        for key in ("sourceId", "sourceSha256", "sourceCamera", "analysisMapping", "revision"):
            assert info(name)[key] == base[key], (name, key)
    assert info("unavailable")["analysisView"]["status"] == "unavailable"
    assert info("unavailable")["analysisView"]["reason"]
    assert info("reload")["revision"] == 3 and info("reload")["sourceSize"] == [1000, 1500]
    for name in names:
        view = info(name)
        log = (generated/f"prompt18-{name}.log").read_text(encoding="utf-8")
        assert "Completed frames=" in log
        assert ("Validation summary: disabled" if name == "release" else "Validation summary: errors=0 warnings=0") in log, name
        if name == "reload":
            assert log.count("Reconstruction Ready:") == 2 and log.count("Source session committed:") == 2
        if name == "views":
            assert all("Image debug switch: "+v in log for v in views)
        if name in ("ui", "warp-resize", "views"):
            assert "Resize: 960x540" in log and "Resize: 1280x720" in log
        if view["workMode"] == "image":
            assert "Source image output: RGBA8 UNORM" in log
        count = 0
        root = Path(view.get("packageRoot", ""))
        if (root/"analysis/analysis.json").exists():
            maps = read(root/"analysis/analysis.json")["maps"]
            readback_dir = generated/f"prompt18-{name}-maps"
            readbacks = read(readback_dir/"readback.json")
            for key, metadata in maps.items():
                if metadata["status"] != "available":
                    continue
                fmt, values = read_dds(root/metadata["path"])
                assert (readback_dir/(key+".bin")).read_bytes() == values.tobytes(), (name, key)
                numeric = readbacks[key]
                assert numeric["stateBefore"] == numeric["stateAfter"] == 128 and numeric["stateCapture"] == 2048
                assert numeric["nonfinite"] == 0 and numeric["byteExact"]
                count += 1
        report["windows"][name] = {"debugLayer": "disabled" if name == "release" else "errors=0 warnings=0", "byteExactMaps": count, "view": view["imageView"]}
    for kind in ("plane", "tilted", "step"):
        for device in ("warp", "hardware"):
            directory = generated/f"prompt18-numeric-{kind}-{device}"
            results = read(directory/"results.json")
            assert "Validation summary: errors=0 warnings=0" in (directory/"gpu.log").read_text(encoding="utf-8")
            assert results[0]["nativeMaxError"] < 2e-5 and results[0]["normalMaxLengthError"] < 1e-5
            for key, npz_key in (("depth", "depth"), ("normal", "normal"), ("position", "point_map")):
                _, values = read_dds(generated/f"prompt18/{kind}/analysis/{key}.dds")
                assert (directory/f"{kind}-maps/{key}.bin").read_bytes() == values.tobytes()
                with np.load(generated/f"prompt18/{kind}/debug/geometry.npz") as saved:
                    np.testing.assert_array_equal(values[..., :3] if values.ndim == 3 else values, saved[npz_key])
            report["numeric"][kind+"-"+device] = results[0]
    labels = read_dds(args.real/"analysis/region.dds")[1]
    palette = np.stack((40+(labels*73)%191, 40+(labels*127)%191, 40+(labels*167)%191), -1).astype(np.int16)
    palette[labels == 0] = 0
    compare("nativeRegionIDs", capture("real-region"), palette, 1)
    # Numeric colors and reflectance take different display paths; verify the actual shader outputs.
    albedo = read_dds(args.real/"analysis/albedo.dds")[1][..., :3]
    encoded = np.where(albedo <= .0031308, albedo*12.92, 1.055*np.maximum(albedo, 0)**(1/2.4)-.055)
    compare("linearAlbedoEncoding", capture("real-albedo"), np.rint(encoded*255).astype(np.int16), 1)
    normal_rgb = read_dds(args.real/"analysis/normal.dds")[1][..., :3]
    length = np.linalg.norm(normal_rgb, axis=-1, keepdims=True)
    normal_rgb = np.divide(normal_rgb, length, out=np.zeros_like(normal_rgb), where=length>1e-6)
    expected = np.rint((normal_rgb*.5+.5)*255).astype(np.int16)
    expected[read_dds(args.real/"analysis/validity.dds")[1] == 0] = [64, 0, 64]
    compare("signedNormalDisplay", capture("real-geometry-normal"), expected, 1)
    for key in ("roughness", "metallic"):
        gray = np.rint(read_dds(args.real/f"analysis/{key}.dds")[1]*255).astype(np.int16)
        compare("linear-"+key, capture("real-"+key), np.repeat(gray[..., None], 3, -1), 1)
    # Exporting saved predictions is a repackaging operation, not another inference/remesh.
    old = generated/"scene13"
    for source in old.rglob("*"):
        if source.is_file():
            relative = source.relative_to(old)
            assert source.read_bytes() == (args.real/relative).read_bytes(), relative
            report["preserved"].append(relative.as_posix())
    validity = read_dds(args.real/"analysis/validity.dds")[1]
    normal = read_dds(args.real/"analysis/normal.dds")[1]
    report["realData"] = {"analysisSize": list(validity.shape[::-1]), "invalidPixels": int((validity == 0).sum()),
        "normalMaxLengthError": float(np.abs(np.linalg.norm(normal[..., :3][validity != 0], axis=-1)-1).max()),
        "regionIDs": np.unique(labels).tolist(), "sourceKind": read(args.real/"relighting/relighting.json")["sourceKind"]}
    (generated/"prompt18-results.json").write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False)+"\n", encoding="utf-8")
    print(json.dumps({"windows": len(names), "pixels": report["pixels"], "numeric": report["numeric"], "realData": report["realData"]}, indent=2))


if __name__ == "__main__":
    main()
