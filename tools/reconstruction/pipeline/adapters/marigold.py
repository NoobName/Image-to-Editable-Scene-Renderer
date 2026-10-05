"""Official Marigold IID Appearance via Diffusers; no model-specific code outside this adapter."""
import gc
import numpy as np
from PIL import Image
from ..material_estimation_backend import MaterialEstimationBackend, MaterialEstimate, srgb_to_linear, validate_material
from .marigold_config import checkpoint_path, MODEL_ID, MODEL_REVISION, WEIGHT_HASHES


def decode_prediction(image, prediction, uncertainty, properties, metadata):
    """Decode checkpoint channel semantics explicitly; never use visualization colormaps as data."""
    if (properties.get("target_names") != ["albedo", "material"] or
        properties.get("albedo", {}).get("prediction_space") != "srgb" or
        properties.get("material", {}).get("sub_target_names") != ["roughness", "metallicity", None] or
        any(properties.get(key, {}).get("prediction_space") != "linear" for key in ("roughness", "metallicity"))):
        raise ValueError("Unsupported Marigold material parameterization")
    values = np.asarray(prediction, dtype=np.float32)
    expected = (2, image.height, image.width, 3)
    if values.shape != expected or not np.isfinite(values).all() or (values < 0).any() or (values > 1).any():
        raise ValueError("Marigold returned invalid material arrays")
    if uncertainty is None:
        confidence = np.zeros((image.height, image.width), np.float32)
        semantics = "zero: ensemble uncertainty unavailable; not a confidence estimate"
    else:
        spread = np.asarray(uncertainty, dtype=np.float32)
        if spread.shape != expected or not np.isfinite(spread).all() or (spread < 0).any() or (spread > 1).any():
            raise ValueError("Marigold returned invalid uncertainty")
        # Ignore the unused material B channel. Normal is not estimated by this checkpoint.
        confidence = 1 - np.maximum(spread[0].max(axis=2), spread[1, :, :, :2].max(axis=2))
        semantics = "1 - max ensemble standard deviation over albedo RGB / roughness / metallic; not calibrated accuracy; excludes fallback normal"
    normal = np.zeros((image.height, image.width, 3), np.float32)
    normal[:, :, 2] = 1
    result = MaterialEstimate(srgb_to_linear(values[0]), values[1, :, :, 0].copy(), values[1, :, :, 1].copy(),
        normal, confidence.astype(np.float32), {**metadata, "albedo_source": "intrinsic", "normal_space": "tangent",
        "normal_source": "flat-tangent-fallback", "normal_confidence": 0, "roughness_source": "estimated",
        "metallic_source": "estimated", "fallbacks": ["normal"], "confidence_semantics": semantics, "dummy": False})
    validate_material(image, result)
    return result


class MarigoldMaterialBackend(MaterialEstimationBackend):
    name = "marigold-iid-appearance"

    def __init__(self, device="auto", offline=False, resolution=512, steps=4, ensemble=3, seed=13):
        if device not in ("auto", "cuda", "cpu") or not 128 <= resolution <= 1024 or not 1 <= steps <= 50 or not 1 <= ensemble <= 8 or not 0 <= seed < 2**32:
            raise ValueError("Invalid material inference options: resolution 128..1024, steps 1..50, ensemble 1..8, uint32 seed")
        self.device, self.offline = device, offline
        self.resolution, self.steps, self.ensemble, self.seed = resolution, steps, ensemble, seed
        self._pipe = None

    def predict(self, image):
        try:
            import torch
            import diffusers
            import transformers
            import accelerate
        except ImportError as error:
            raise RuntimeError("Material inference dependencies missing; run setup-material.ps1 in the project environment") from error
        device = ("cuda" if torch.cuda.is_available() else "cpu") if self.device == "auto" else self.device
        if device == "cuda" and not torch.cuda.is_available():
            raise RuntimeError("Marigold requested CUDA but it is unavailable")
        path = checkpoint_path(self.offline)
        print(f"Marigold IID Appearance: {device}, {self.steps} steps, ensemble {self.ensemble}, resolution {self.resolution}", flush=True)
        try:
            pipe = diffusers.MarigoldIntrinsicsPipeline.from_pretrained(str(path), variant="fp16", use_safetensors=True,
                torch_dtype=torch.float16 if device == "cuda" else torch.float32, local_files_only=True)
            self._pipe = pipe
            if device == "cuda":
                pipe.enable_model_cpu_offload()  # Keep peak VRAM suitable for the project's 8 GB GPU.
            else:
                pipe.to("cpu")
            pipe.vae.enable_slicing()
            with torch.inference_mode():
                output = pipe(Image.fromarray(image.rgb), num_inference_steps=self.steps, ensemble_size=self.ensemble,
                    processing_resolution=self.resolution, match_input_resolution=True, batch_size=1,
                    generator=torch.Generator(device=device).manual_seed(self.seed),
                    output_uncertainty=self.ensemble >= 3, ensembling_kwargs={"reduction": "mean"}, output_type="np")
            metadata = {"backend": self.name, "model_id": MODEL_ID, "model_revision": MODEL_REVISION,
                "weight_sha256": WEIGHT_HASHES, "device": device, "seed": self.seed, "steps": self.steps,
                "ensemble": self.ensemble, "processing_resolution": self.resolution,
                "torch_version": torch.__version__, "diffusers_version": diffusers.__version__,
                "transformers_version": transformers.__version__, "accelerate_version": accelerate.__version__,
                "license": "Apache-2.0 code / RAIL++-M weights; see official model card",
                "limitations": ["Estimated reflectance, not measured ground truth; shadows/specular residuals can remain.",
                                "No material detail normal prediction; geometry normals remain unchanged."]}
            return decode_prediction(image, output.prediction, output.uncertainty, dict(pipe.target_properties), metadata)
        except torch.cuda.OutOfMemoryError as error:
            raise RuntimeError("Marigold exhausted GPU memory; lower --material-resolution or use --device cpu") from error
        finally:
            self.release()

    def release(self):
        if self._pipe is not None:
            self._pipe.remove_all_hooks()
            self._pipe = None
            gc.collect()
            import torch
            if torch.cuda.is_available():
                torch.cuda.empty_cache()
