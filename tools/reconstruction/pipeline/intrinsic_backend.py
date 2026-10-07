"""Model-independent intrinsic data. None residual means unavailable, never inferred specular."""
from abc import ABC,abstractmethod
from dataclasses import dataclass
import numpy as np
from .material_estimation_backend import srgb_to_linear

@dataclass(frozen=True)
class IntrinsicEstimate:
    albedo: np.ndarray
    shading: np.ndarray
    residual: np.ndarray | None
    uncertainty: np.ndarray # RGB channels mean A/S/R maximum RGB spread, not a probability.
    validity: np.ndarray
    metadata: dict

class IntrinsicBackend(ABC):
    @abstractmethod
    def predict(self,image,material=None):...
    def release(self):pass

def validate_intrinsic(image,result):
    shape=(image.height,image.width,3)
    for name,lo,hi in (('albedo',0,1),('shading',0,64),('residual',-64,64),('uncertainty',0,64)):
        a=getattr(result,name)
        if name=='residual' and a is None:continue
        if not isinstance(a,np.ndarray) or a.dtype!=np.float32 or a.shape!=shape or not np.isfinite(a).all() or np.any(a<lo) or np.any(a>hi):
            raise ValueError(f'Invalid intrinsic {name}: expected finite float32 {shape} in [{lo},{hi}]')
    if result.validity.dtype!=np.uint32 or result.validity.shape!=shape[:2] or np.any(result.validity>1):raise ValueError('Intrinsic validity must be uint32 binary')
    if result.metadata.get('residual_semantics') not in ('signed-non-diffuse','nonnegative-non-diffuse','unavailable'):raise ValueError('Intrinsic residual semantics missing')
    if (result.residual is None)!=(result.metadata['residual_semantics']=='unavailable'):raise ValueError('Residual availability contradicts declared semantics')
    if result.residual is not None and result.metadata['residual_semantics']=='nonnegative-non-diffuse' and np.any(result.residual<0):raise ValueError('Residual contradicts nonnegative declaration')

class ProxyIntrinsicBackend(IntrinsicBackend):
    name='proxy'
    def predict(self,image,material=None):
        # Deterministic wiring baseline, explicitly not a new intrinsic estimate.
        linear=srgb_to_linear(image.rgb.astype(np.float32)/255)
        albedo=getattr(material,'albedo',np.full_like(linear,.5)).copy()
        validity=np.all(albedo>=.03,axis=-1).astype(np.uint32)
        shading=linear/np.maximum(albedo,.03);shading[validity==0]=0
        result=IntrinsicEstimate(albedo,shading.astype(np.float32),None,np.ones_like(linear),validity,
            dict(backend=self.name,residual_semantics='unavailable',uncertainty_semantics='one: unavailable uncertainty; no model run',
                 gauge='proxy-I-over-existing-material-albedo',scale_ambiguous=True,provenance='derived-not-estimated'))
        validate_intrinsic(image,result);return result

class SavedIntrinsicBackend(IntrinsicBackend):
    name='saved'
    def __init__(self,package):self.package=package
    def predict(self,image,material=None):
        from intrinsic_contract import load_intrinsic,read_arrays
        from appearance_contract import load_extension
        data=load_intrinsic(self.package,load_extension(self.package))
        if data is None:raise ValueError('Saved intrinsic sidecar unavailable')
        maps=read_arrays(self.package,data)
        result=IntrinsicEstimate(maps['albedo'][...,:3],maps['shading'][...,:3],maps['residual'][...,:3] if 'residual' in maps else None,
            maps['uncertainty'][...,:3],maps['validity'],{**data['provenance'],'reused_from':str(self.package)})
        validate_intrinsic(image,result);return result
