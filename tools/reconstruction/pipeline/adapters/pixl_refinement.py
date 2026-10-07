"""Pinned, opt-in PIXLRelight adapter. Models live only in this independent process."""
import gc
import hashlib
import json
import sys
import time
import numpy as np
from PIL import Image
from ..neural_refinement_backend import NeuralRefinementBackend, RefinementCandidate
from fetch_refinement import ROOT, VENDOR, MODEL_ID, MODEL_REVISION, CODE_REVISION

WEIGHT_SHA256 = '69c2bd11c2f272754f7080bc33e4b049fd334ea5d596df0c475f51c93699710e'
CONFIG_SHA256 = 'a3ef2cada8a223aee15b2e85d6627980962ea5446242d0d8ec24791ec2e21021'
ADAPTER_REVISION = 'pixl-physics-refinement-34-v2'


def decode(rgb):
    a = np.asarray(rgb, np.float32) / 255
    return np.where(a <= .04045, a/12.92, ((a+.055)/1.055)**2.4).astype(np.float32)


def encode(a):
    a = np.clip(a,0,1)
    return np.round(np.where(a <= .0031308, 12.92*a, 1.055*a**(1/2.4)-.055)*255).astype(np.uint8)


def checkpoint_path():
    path = ROOT / '.cache/huggingface' / ('models--'+MODEL_ID.replace('/','--')) / 'snapshots' / MODEL_REVISION
    # The config selects Python classes. Authenticate it before constructing objects.
    if hashlib.sha256((path/'config.yaml').read_bytes()).hexdigest() != CONFIG_SHA256:
        raise RuntimeError('Optional refinement config SHA256 mismatch')
    with (path/'model.safetensors').open('rb') as stream:
        if hashlib.file_digest(stream,'sha256').hexdigest() != WEIGHT_SHA256:
            raise RuntimeError('Optional refinement checkpoint SHA256 mismatch')
    manifest=json.loads((VENDOR/'source-manifest.json').read_text(encoding='utf-8'))
    for name,digest in manifest.items():
        if hashlib.sha256((VENDOR/name).read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Optional model source changed: '+name)
    return path


class PixlRefinementBackend(NeuralRefinementBackend):
    def __init__(self):
        self.debug_guidance = None
        self.raw_candidate = None

    def predict(self, observation, parameters, cancelled):
        import torch
        from safetensors.torch import load_file
        from omegaconf import OmegaConf
        from .intrinsic_config import checkpoint_path as intrinsic_path, MODEL_REVISION as intrinsic_revision
        def check():
            if cancelled():raise RuntimeError('Refinement cancelled')
        check(); path=checkpoint_path()
        if not torch.cuda.is_available():raise RuntimeError('Validated refinement requires CUDA')
        free,total=torch.cuda.mem_get_info()
        # Measured 256 run needs ~2.8 GB allocated. Leave space for the desktop Renderer.
        required=(3 if parameters.max_side==256 else 4)*1024**3
        if free<required:raise RuntimeError(f'Refinement GPU budget unavailable: free={free}, required={required}')
        sys.path[:0]=[str(ROOT/'.vendor/refinement-deps'),str(VENDOR)]
        from src.models.pixl_relight import PIXLRelight
        from src.utils.cfg import create_object
        from src.rgbx.rgbx import MarigoldIIDPipeline
        start=time.perf_counter(); torch.manual_seed(parameters.seed); torch.cuda.reset_peak_memory_stats()
        pipe=model=prediction=a=b=None
        try:
            h,w=observation.original_rgb.shape[:2]; size=parameters.max_side
            sw=max(16,round(w/max(w,h)*size/16)*16); sh=max(16,round(h/max(w,h)*size/16)*16)
            def tensor(rgb):
                image=np.asarray(Image.fromarray(rgb).resize((sw,sh),Image.Resampling.LANCZOS)).astype(np.float32)/255
                image=np.pad(image,((0,size-sh),(0,size-sw),(0,0)),mode='edge')
                return torch.from_numpy(image).permute(2,0,1)[None].cuda()
            a=tensor(observation.original_rgb); b=tensor(encode(observation.physics_rgb))
            print('Refinement: decomposing fixed physics RGB into target A/S/R',flush=True)
            pipe=MarigoldIIDPipeline.from_pretrained(str(intrinsic_path(True)),variant='fp16',
                torch_dtype=torch.float16,local_files_only=True,use_safetensors=True).to('cuda')
            with torch.inference_mode():
                guidance=pipe(b,denoising_steps=1,processing_res=0,match_input_res=True,batch_size=1,
                    show_progress_bar=False,generator=torch.Generator(device='cuda').manual_seed(parameters.seed)).detach().cpu()
            self.debug_guidance=guidance.numpy(); pipe=None; b=None; gc.collect(); torch.cuda.empty_cache(); check()
            config=OmegaConf.load(path/'config.yaml')
            model=PIXLRelight(create_object(config.model.net),create_object(config.model.head),torch.nn.Identity())
            state=load_file(str(path/'model.safetensors'))
            missing,unexpected=model.load_state_dict(state,strict=False)
            if missing or unexpected:raise RuntimeError('Pinned checkpoint does not match the adapter architecture')
            del state
            model.eval().requires_grad_(False).to(device='cuda',dtype=torch.bfloat16)
            print('Refinement: pretrained PIXLRelight forward',flush=True)
            with torch.inference_mode(),torch.autocast('cuda',dtype=torch.bfloat16):
                prediction=model(a,guidance.cuda())
            torch.cuda.synchronize(); check()
            # Unpad before upsampling gain/bias. Native coordinates must not inherit padded borders.
            gain=torch.nn.functional.interpolate(prediction.rgb_gain[:,:,:sh,:sw].float().cpu(),size=(h,w),mode='bilinear',align_corners=False)
            bias=torch.nn.functional.interpolate(prediction.rgb_bias[:,:,:sh,:sw].float().cpu(),size=(h,w),mode='bilinear',align_corners=False)
            src=torch.from_numpy(observation.original_rgb.astype(np.float32)/255).permute(2,0,1)[None]
            rgb=(gain*src+bias).clamp(0,1)[0].permute(1,2,0).numpy()
            linear=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4).astype(np.float32)
            self.raw_candidate=linear
            return RefinementCandidate(linear,dict(modelId=MODEL_ID,modelRevision=MODEL_REVISION,
                codeRevision=CODE_REVISION,weightSha256=WEIGHT_SHA256,adapterRevision=ADAPTER_REVISION,
                configSha256=CONFIG_SHA256,
                seed=parameters.seed,processingSide=size,alignedSize=[sw,sh],nativeSize=[w,h],
                intrinsicRevision=intrinsic_revision,intrinsicSteps=1,torchVersion=torch.__version__,
                dtype='bfloat16 network; float16 intrinsic; float32 native modulation',seconds=time.perf_counter()-start,
                peakAllocatedBytes=torch.cuda.max_memory_allocated(),peakReservedBytes=torch.cuda.max_memory_reserved(),
                guidance='Original RGB + A/S/R estimated FROM fixed physics RGB; geometry/old-new/masks are external rejection guidance, not network input channels',
                inputEncoding='sRGB display-referred RGB8; output decoded to display-linear, not HDR radiance',
                license='CC BY-NC 4.0 weights; MIT root code with inherited per-file notices'))
        finally:
            pipe=model=prediction=a=b=None; gc.collect(); torch.cuda.empty_cache()
