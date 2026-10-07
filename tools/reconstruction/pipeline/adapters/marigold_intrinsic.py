"""Lighting-specific adapter; its albedo is already LINEAR, unlike Appearance."""
import gc
import time
import numpy as np
from PIL import Image
from ..intrinsic_backend import IntrinsicBackend,IntrinsicEstimate,validate_intrinsic
from .intrinsic_config import checkpoint_path,MODEL_ID,MODEL_REVISION,WEIGHT_HASHES

def decode_intrinsic(image,prediction,uncertainty,properties,metadata):
    if properties.get('target_names')!=['albedo','shading','residual'] or any(properties.get(k,{}).get('prediction_space')!='linear' for k in ('albedo','shading','residual')):
        raise ValueError('Unsupported Lighting target_properties; expected linear A/S/R')
    if properties['albedo'].get('up_to_scale') is not False or any(properties[k].get('up_to_scale') is not True for k in ('shading','residual')):raise ValueError('Unexpected Lighting scale contract')
    a=np.asarray(prediction,dtype=np.float32)
    if a.shape!=(3,image.height,image.width,3) or not np.isfinite(a).all() or np.any(a<0) or np.any(a>1):raise ValueError('Invalid Lighting prediction channels/range')
    spread=np.ones((image.height,image.width,3),np.float32)
    semantics='one: uncertainty unavailable (ensemble<3)'
    if uncertainty is not None:
        u=np.asarray(uncertainty,dtype=np.float32)
        if u.shape!=a.shape or not np.isfinite(u).all() or np.any(u<0) or np.any(u>1):raise ValueError('Invalid Lighting uncertainty')
        spread=np.moveaxis(u.max(axis=-1),0,-1).copy();semantics='A/S/R maximum RGB ensemble std; native linear model units; not calibrated accuracy'
    result=IntrinsicEstimate(a[0].copy(),a[1].copy(),a[2].copy(),spread,np.ones((image.height,image.width),np.uint32),
        {**metadata,'backend':'marigold-lighting','target_properties':properties,'residual_semantics':'nonnegative-non-diffuse',
         'uncertainty_semantics':semantics,'gauge':'checkpoint-native-no-renormalization','scale_ambiguous':True,'provenance':'estimated'})
    validate_intrinsic(image,result);return result

class MarigoldIntrinsicBackend(IntrinsicBackend):
    name='marigold-lighting'
    def __init__(self,device='auto',offline=False,resolution=512,steps=4,ensemble=3,seed=23):
        if device not in ('auto','cuda','cpu') or not 128<=resolution<=1024 or not 1<=steps<=50 or not 1<=ensemble<=8 or not 0<=seed<2**32:raise ValueError('Invalid intrinsic inference options')
        self.device,self.offline,self.resolution,self.steps,self.ensemble,self.seed=device,offline,resolution,steps,ensemble,seed;self._pipe=None
    def predict(self,image,material=None):
        import torch,diffusers,transformers,accelerate
        device=('cuda' if torch.cuda.is_available() else 'cpu') if self.device=='auto' else self.device
        if device=='cuda' and not torch.cuda.is_available():raise RuntimeError('Intrinsic CUDA unavailable')
        checkpoint=checkpoint_path(self.offline)
        print(f'Intrinsic Lighting: {device}, resolution={self.resolution}, steps={self.steps}, ensemble={self.ensemble}',flush=True)
        start=time.perf_counter();pipe=None;module=None;output=None
        if device=='cuda':torch.cuda.reset_peak_memory_stats()
        try:
            pipe=diffusers.MarigoldIntrinsicsPipeline.from_pretrained(str(checkpoint),variant='fp16',use_safetensors=True,torch_dtype=torch.float16 if device=='cuda' else torch.float32,local_files_only=True)
            self._pipe=pipe
            for module in (pipe.unet,pipe.vae,pipe.text_encoder):module.requires_grad_(False);module.eval()
            if device=='cuda':pipe.enable_model_cpu_offload()
            else:pipe.to('cpu')
            pipe.vae.enable_slicing()
            with torch.inference_mode():
                output=pipe(Image.fromarray(image.rgb),num_inference_steps=self.steps,ensemble_size=self.ensemble,processing_resolution=self.resolution,
                    match_input_resolution=True,batch_size=1,generator=torch.Generator(device=device).manual_seed(self.seed),output_uncertainty=self.ensemble>=3,
                    ensembling_kwargs={'reduction':'mean'},output_type='np')
            if device=='cuda':torch.cuda.synchronize()
            metadata=dict(model_id=MODEL_ID,model_revision=MODEL_REVISION,weight_sha256=WEIGHT_HASHES,device=device,device_name=torch.cuda.get_device_name() if device=='cuda' else 'CPU',
                seed=self.seed,steps=self.steps,ensemble=self.ensemble,processing_resolution=self.resolution,analysis_size=[image.width,image.height],
                seconds=time.perf_counter()-start,peak_allocated_bytes=torch.cuda.max_memory_allocated() if device=='cuda' else 0,peak_reserved_bytes=torch.cuda.max_memory_reserved() if device=='cuda' else 0,
                torch_version=torch.__version__,diffusers_version=diffusers.__version__,transformers_version=transformers.__version__,accelerate_version=accelerate.__version__,
                license='RAIL++-M weights; Apache-2.0 code; see pinned official model card',frozen=True)
            return decode_intrinsic(image,output.prediction,output.uncertainty,dict(pipe.target_properties),metadata)
        except torch.cuda.OutOfMemoryError as error:raise RuntimeError('Intrinsic GPU memory exhausted; lower --resolution or use CPU') from error
        finally:
            # Drop local references before empty_cache; a hook removal alone does not free the local pipeline.
            pipe=None;module=None;output=None;self.release()
    def release(self):
        if self._pipe is not None:
            self._pipe.remove_all_hooks();self._pipe=None
        # Also collect after predict returns: Diffusers generators can keep small temporary cycles alive.
        gc.collect()
        import torch
        if torch.cuda.is_available():torch.cuda.empty_cache()
