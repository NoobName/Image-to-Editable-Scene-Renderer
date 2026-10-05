"""Official SAM 2.1 Tiny behind the project CPU mask-proposal interface."""
from contextlib import nullcontext
import hashlib
from pathlib import Path
import sys
import numpy as np
from ..segmentation_backend import SegmentationBackend
from ..segmentation_types import MaskProposal, SegmentationPrediction
from ..segmentation_prompts import load_prompts
from .sam2_config import ROOT,SOURCE_DIR,SOURCE_REVISION,MODEL_CONFIG,MODEL_FILE,MODEL_ID,MODEL_REVISION,MODEL_SHA256


class Sam2SegmentationBackend(SegmentationBackend):
    name = "sam2"

    def __init__(self,device="auto",offline=False,prompts=None,points_per_side=16):
        if device not in ("auto","cpu","cuda") or not isinstance(points_per_side,int) or not 4<=points_per_side<=32:
            raise ValueError("SAM 2 device must be auto/cpu/cuda; points-per-side must be 4..32")
        self.device,self.offline,self.points_per_side = device,offline,points_per_side
        self.prompts = load_prompts(prompts) if prompts else None
        self._model = None
        self._metadata = {}

    def _load(self):
        if self._model is not None:
            return
        if not SOURCE_DIR.is_dir():
            raise RuntimeError("SAM 2 sources missing; run tools/reconstruction/setup-sam2.ps1 in the project environment")
        if str(SOURCE_DIR) not in sys.path:
            sys.path.insert(0,str(SOURCE_DIR))
        try:
            import torch
            import torchvision
            import sam2
            from sam2.build_sam import build_sam2
            from huggingface_hub import hf_hub_download
        except ImportError as error:
            raise RuntimeError(f"SAM 2 dependency missing: {error}; run setup-sam2.ps1") from error
        if not Path(sam2.__file__).resolve().is_relative_to(SOURCE_DIR.resolve()):
            raise RuntimeError("A different SAM 2 package is already loaded; use a fresh project Python process")
        device = ("cuda" if torch.cuda.is_available() else "cpu") if self.device=="auto" else self.device
        if device=="cuda" and not torch.cuda.is_available():
            raise RuntimeError("SAM 2 requested CUDA but it is unavailable")
        path = Path(hf_hub_download(MODEL_ID,MODEL_FILE,revision=MODEL_REVISION,
                                   cache_dir=str(ROOT/".cache/huggingface"),local_files_only=self.offline))
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream,"sha256").hexdigest()
        if digest!=MODEL_SHA256:
            raise RuntimeError("SAM 2 checkpoint SHA256 mismatch")
        print(f"SAM 2.1 Tiny: loading on {device}; {'named prompts' if self.prompts else 'automatic masks'}",flush=True)
        self._model = build_sam2(MODEL_CONFIG,str(path),device=device,apply_postprocessing=False)
        self._metadata = {"model_id":MODEL_ID,"model_revision":MODEL_REVISION,"checkpoint_sha256":digest,
                          "source_revision":SOURCE_REVISION,"device":device,"torch_version":torch.__version__,
                          "torchvision_version":torchvision.__version__,"prompted":self.prompts is not None,
                          "points_per_side":self.points_per_side,"dummy":False,
                          "semantic_labels":"provided prompts or downstream geometry heuristics, not SAM class predictions",
                          "optional_cuda_postprocessing":False}

    def release(self):
        if self._model is not None:
            self._model = None
            import gc
            import torch
            gc.collect()
            if torch.cuda.is_available():
                torch.cuda.empty_cache()

    def predict(self,image):
        self._load()
        import torch
        from sam2.sam2_image_predictor import SAM2ImagePredictor
        from sam2.automatic_mask_generator import SAM2AutomaticMaskGenerator
        device = self._metadata["device"]
        autocast = torch.autocast("cuda",dtype=torch.bfloat16) if device=="cuda" else nullcontext()
        proposals = []
        try:
            with torch.inference_mode(),autocast:
                if self.prompts:
                    predictor = SAM2ImagePredictor(self._model,max_hole_area=0,max_sprinkle_area=0)
                    predictor.set_image(image.rgb.copy())
                    for prompt in self.prompts:
                        points = prompt.get("points",[])
                        # Normalized positions refer to processed image bounds, as in SAM's API.
                        coords = np.array([p[:2] for p in points],np.float32)*[image.width,image.height] if points else None
                        labels = np.array([p[2] for p in points],np.int32) if points else None
                        box = np.array(prompt["box"],np.float32)*[image.width,image.height,image.width,image.height] if "box" in prompt else None
                        masks,scores,_ = predictor.predict(point_coords=coords,point_labels=labels,box=box,multimask_output=True)
                        best = int(np.argmax(scores))
                        mask = np.asarray(masks[best],dtype=bool)
                        if not mask.any():
                            raise ValueError(f"SAM 2 produced an empty mask for {prompt['name']}; adjust its prompt")
                        proposals.append(MaskProposal(mask,float(np.clip(scores[best],0,1)),prompt["id"],prompt["name"],prompt["category"]))
                else:
                    generator = SAM2AutomaticMaskGenerator(self._model,points_per_side=self.points_per_side,
                        points_per_batch=16,pred_iou_thresh=.8,stability_score_thresh=.95,crop_n_layers=0,min_mask_region_area=0)
                    for item in generator.generate(image.rgb.copy()):
                        proposals.append(MaskProposal(np.asarray(item["segmentation"],dtype=bool),float(np.clip(item["predicted_iou"],0,1))))
        except torch.cuda.OutOfMemoryError as error:
            raise RuntimeError("SAM 2 exhausted GPU memory; close GPU applications or select --device cpu") from error
        if not proposals:
            raise RuntimeError("SAM 2 found no masks; provide point/box prompts instead of silently falling back to Dummy")
        return SegmentationPrediction(tuple(proposals),self._metadata.copy())
