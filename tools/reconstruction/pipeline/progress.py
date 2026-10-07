"""Versioned, atomic progress snapshots; no sockets or renderer imports."""
import json
import os
import time
from pathlib import Path

LEGACY_STAGES = ("geometry", "segmentation", "materials", "export")
STAGES = ("geometry", "segmentation", "materials", "lighting", "export")
OFFLINE_STAGES = ("lighting", "export")
INTRINSIC_STAGES = ("intrinsic", "export")
SHADOW_STAGES = ("shadow", "export")


class ProgressReporter:
    def __init__(self, path=None, job_id="", stages=STAGES, version=2):
        if version not in (1, 2) or tuple(stages) not in (STAGES, OFFLINE_STAGES, LEGACY_STAGES, INTRINSIC_STAGES, SHADOW_STAGES):
            raise ValueError("Unsupported progress stage table")
        self.stages = LEGACY_STAGES if version == 1 else tuple(stages)
        self.path = Path(path) if path else None
        self.snapshot = {"version": version, "job_id": job_id, "sequence": 0, "state": "processing",
                         "stages": {key: "pending" for key in self.stages}, "detail": "Starting Python pipeline"}
        if version == 2:
            self.snapshot["stage_order"] = list(self.stages)
        self.publish()

    def publish(self):
        if self.path is None:
            return
        self.snapshot["sequence"] += 1
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_name(self.path.name + ".tmp")
        temporary.write_text(json.dumps(self.snapshot, ensure_ascii=False, allow_nan=False), encoding="utf-8")
        for attempt in range(10):
            try:
                os.replace(temporary, self.path)
                break
            except PermissionError:
                if attempt == 9:
                    raise
                time.sleep(.02)  # Bounded retry for transient Windows scanner/file-reader contention.

    def stage(self, name, state, detail=""):
        if name not in self.stages or state not in ("running", "complete"):
            raise ValueError("Invalid reconstruction progress event")
        self.snapshot["stages"][name] = state
        self.snapshot["detail"] = detail or f"{name}: {state}"
        self.publish()

    def finish(self):
        if any(value != "complete" for value in self.snapshot["stages"].values()):
            raise ValueError("Cannot finish an incomplete reconstruction")
        self.snapshot["state"] = "complete"
        self.snapshot["detail"] = "ScenePackage published"
        self.publish()

    def fail(self, error):
        for name, state in self.snapshot["stages"].items():
            if state == "running":
                self.snapshot["stages"][name] = "error"
        self.snapshot["state"] = "error"
        self.snapshot["detail"] = str(error)
        self.publish()
