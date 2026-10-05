"""Official MoGe-2 adapter: OpenCV camera coordinates -> renderer LH camera space."""
from pathlib import Path
import sys
import hashlib
import numpy as np
from ..geometry_backend import GeometryEstimationBackend, GeometryPrediction, validate_prediction
from .moge_config import ROOT, SOURCES, MOGE_REVISION, MODEL_ID, MODEL_REVISION, MODEL_SHA256


def convert_output(image, output, metadata):
    """Pure NumPy conversion, separately testable without model downloads or torch."""
    required = {"points", "depth", "normal", "intrinsics", "mask"}
    if not required.issubset(output):
        raise ValueError(f"MoGe-2 normal checkpoint must return {sorted(required)}")
    h, w = image.height, image.width
    expected = {"points": (h, w, 3), "normal": (h, w, 3), "depth": (h, w), "mask": (h, w), "intrinsics": (3, 3)}
    if any(np.asarray(output[name]).shape != shape for name, shape in expected.items()):
        raise ValueError("MoGe returned incompatible output dimensions")
    points = np.array(output["points"], dtype=np.float32, copy=True)
    normals = np.array(output["normal"], dtype=np.float32, copy=True)
    depth = np.array(output["depth"], dtype=np.float32, copy=True)
    raw_mask = np.asarray(output["mask"])
    if raw_mask.dtype != np.bool_:
        raise ValueError("Expected official MoGe infer() boolean validity mask")
    # Reflection of Y changes OpenCV (right/down/forward) to our LH (right/up/forward).
    points[..., 1] *= -1
    normals[..., 1] *= -1
    length = np.linalg.norm(normals, axis=-1)
    valid = raw_mask & np.isfinite(depth) & (depth > 0) & (depth <= 10000)
    valid &= np.isfinite(points).all(-1) & np.isfinite(normals).all(-1) & (length > 1e-6)
    normals[valid] /= length[valid, None]
    points[~valid], normals[~valid], depth[~valid] = 0, 0, 0
    result = GeometryPrediction(depth, normals, points, np.asarray(output["intrinsics"], dtype=np.float32),
                                valid.astype(np.float32), valid, "metric",
                                {**metadata, "backend": "moge", "dummy": False, "confidence_kind": "binary-validity-not-calibrated"})
    validate_prediction(image, result)
    return result


class MoGeGeometryBackend(GeometryEstimationBackend):
    name = "moge"

    def __init__(self, device="auto", checkpoint=None, offline=False, resolution_level=0):
        if device not in ("auto", "cpu", "cuda"):
            raise ValueError("device must be auto, cpu or cuda")
        if not 0 <= resolution_level <= 9:
            raise ValueError("resolution-level must be in [0,9]")
        self.device, self.checkpoint, self.offline = device, checkpoint, offline
        self.resolution_level = resolution_level
        self._model = None
        self._metadata = {}

    def _load(self):
        if self._model is not None:
            return
        for _, directory, _ in SOURCES:
            source = ROOT / ".vendor" / directory
            if not source.is_dir():
                raise RuntimeError("MoGe sources missing. Run tools/reconstruction/setup-moge.ps1 first.")
            if str(source) not in sys.path:
                sys.path.insert(0, str(source))
        try:
            import torch
            from huggingface_hub import hf_hub_download
            from moge.model.v2 import MoGeModel
        except ImportError as error:
            raise RuntimeError(f"MoGe dependency missing: {error}. Run tools/reconstruction/setup-moge.ps1.") from error
        device = ("cuda" if torch.cuda.is_available() else "cpu") if self.device == "auto" else self.device
        if device == "cuda" and not torch.cuda.is_available():
            raise RuntimeError("CUDA was requested but is unavailable. Install CUDA PyTorch or use --device cpu.")
        if self.checkpoint:
            path = Path(self.checkpoint).resolve(strict=True)
        else:
            path = Path(hf_hub_download(MODEL_ID, "model.pt", revision=MODEL_REVISION,
                                       cache_dir=str(ROOT / ".cache" / "huggingface"), local_files_only=self.offline))
        with path.open("rb") as file:
            digest = hashlib.file_digest(file, "sha256").hexdigest()
        if not self.checkpoint and digest != MODEL_SHA256:
            raise RuntimeError("Official checkpoint SHA256 mismatch; refusing to load")
        print(f"MoGe-2: loading {path.name} on {device}; resolution-level={self.resolution_level}", flush=True)
        # Official v2 from_pretrained uses strict=False. Require a complete matching checkpoint,
        # so a local incompatible checkpoint cannot silently leave randomly initialized layers.
        checkpoint_data = torch.load(path, map_location="cpu", weights_only=True)
        model = MoGeModel(**checkpoint_data["model_config"])
        model.load_state_dict(checkpoint_data["model"], strict=True)
        if not hasattr(model, "normal_head") or not hasattr(model, "scale_head"):
            raise ValueError("Use a MoGe-2 checkpoint with both normal and metric scale heads")
        self._model = model.to(device).eval()
        self._metadata = {"model_id": MODEL_ID if not self.checkpoint else "local-moge-2-checkpoint",
                          "model_revision": MODEL_REVISION if not self.checkpoint else None,
                          "checkpoint_sha256": digest, "source_revision": MOGE_REVISION,
                          "device": device, "torch_version": torch.__version__, "resolution_level": self.resolution_level}

    def release(self):
        if self._model is not None:
            self._model = None
            import gc
            import torch
            gc.collect()
            if torch.cuda.is_available():
                torch.cuda.empty_cache()

    def predict(self, image):
        if min(image.width, image.height) < 16 or not .5 <= image.width/image.height <= 2:
            raise ValueError("MoGe input must be at least 16 pixels per side, with aspect ratio between 1:2 and 2:1")
        self._load()
        import torch
        tensor = torch.from_numpy(image.rgb.copy()).to(self._model.device, dtype=torch.float32).permute(2, 0, 1)/255
        try:
            with torch.inference_mode():
                result = self._model.infer(tensor, resolution_level=self.resolution_level,
                                           apply_mask=False, force_projection=True,
                                           use_fp16=self._model.device.type == "cuda")
            arrays = {name: value.detach().float().cpu().numpy() if name != "mask" else value.detach().cpu().numpy()
                      for name, value in result.items()}
            return convert_output(image, arrays, self._metadata)
        except torch.cuda.OutOfMemoryError as error:
            raise RuntimeError("MoGe ran out of GPU memory. Close GPU applications, lower --resolution-level, or use --device cpu.") from error
