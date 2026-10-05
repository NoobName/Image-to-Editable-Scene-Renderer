"""Check actual desktop run logs and compare hot-import vs startup rendering."""
import json
from pathlib import Path
import re
import numpy as np
from PIL import Image


def main():
    root = Path(__file__).resolve().parents[2] / "generated"
    report = {}
    cases = ("full-fixed", "repeat", "release-unicode", "cancel", "missing-image", "python-error", "final", "final-cold", "warp")
    for name in cases:
        log = (root / f"prompt14-{name}.log").read_text(encoding="utf-8")
        expected = "disabled (Release" if name == "release-unicode" else "errors=0 warnings=0"
        if expected not in log or "nonfinite=0" not in log:
            raise AssertionError(f"Invalid GPU/validation result: {name}")
        error = name in ("cancel", "missing-image", "python-error")
        if error and ("Reconstruction Error:" not in log or "GPU scene committed" in log):
            raise AssertionError(f"Failure replaced the old scene or was not reported: {name}")
        if not error and name != "final-cold" and "Reconstruction Ready:" not in log:
            raise AssertionError(f"No loaded scene: {name}")
        transitions = [(state, int(frame)) for state, frame in re.findall(r"Reconstruction state (\w+) at rendered frame=(\d+)", log)]
        report[name] = {"transitions": transitions, "commits": log.count("GPU scene committed")}
    if report["repeat"]["commits"] != 2:
        raise AssertionError("Two successive imports were not observed")
    for name in ("repeat", "final", "warp"):
        transitions = report[name]["transitions"]
        loading = next(frame for state, frame in transitions if state == "Loading")
        ready = next(frame for state, frame in transitions if state == "Ready")
        if ready <= loading + 2:
            raise AssertionError(f"No evidence of ongoing rendering during Loading: {name}")
    hot = np.asarray(Image.open(root / "prompt14-final.bmp").convert("RGB"), np.int16)
    cold = np.asarray(Image.open(root / "prompt14-final-cold.bmp").convert("RGB"), np.int16)
    if hot.shape != cold.shape:
        raise AssertionError("Capture shape mismatch")
    difference = np.abs(hot - cold)
    report["hot_vs_cold"] = {"maximum_byte_difference": int(difference.max()), "changed_pixels": int(np.any(difference, axis=2).sum())}
    if difference.max() != 0:
        raise AssertionError(f"Background import changed the rendered scene: {report['hot_vs_cold']}")
    report["verified_windows"] = len(cases)
    (root / "prompt14-validation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
