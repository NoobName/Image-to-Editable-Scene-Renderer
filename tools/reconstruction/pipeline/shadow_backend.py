"""Explainable old-shadow support from fixed observations; never changes rendered RGB."""
from abc import ABC,abstractmethod
from dataclasses import dataclass
import numpy as np
from .lighting_backend import LUMA,coefficients

@dataclass(frozen=True)
class ShadowInput:
    depth:np.ndarray
    normal:np.ndarray
    points:np.ndarray
    intrinsics:np.ndarray
    valid:np.ndarray
    labels:np.ndarray
    roughness:np.ndarray
    metallic:np.ndarray
    albedo:np.ndarray
    shading:np.ndarray
    error:np.ndarray
    uncertainty:np.ndarray
    excluded:np.ndarray
    light:dict
    shading_scale:float
    light_confidence:float

class ShadowEstimationBackend(ABC):
    @abstractmethod
    def predict(self,observation):...

def geometry_support(o,eligible,steps=96,bias_relative=.01,thickness_relative=.06,max_distance_scale=2.):
    """Camera-depth shell ray support, not complete 3D visibility. Different region required.

    Missing/offscreen samples cannot establish an occluder; an empty ray is unknown, not lit truth.
    Requiring two shell hits avoids a single quantized sample being treated as evidence.
    """
    h,w=o.depth.shape;flat=np.flatnonzero(eligible);support=np.zeros(h*w,np.float32)
    if not len(flat):return support.reshape(h,w)
    scale=float(np.median(o.depth[o.valid]));direction=-np.asarray(o.light['direction'],float);direction/=np.linalg.norm(direction)
    for start in range(0,len(flat),8192):
        ids=flat[start:start+8192];p=o.points.reshape(-1,3)[ids];labels=o.labels.reshape(-1)[ids];hits=np.zeros(len(ids),np.uint16)
        for distance in np.linspace(scale*bias_relative*2,scale*max_distance_scale,steps):
            q=p+distance*direction;z=q[:,2];safe=np.maximum(z,1e-6)
            u=o.intrinsics[0,0]*q[:,0]/safe+o.intrinsics[0,2];v=-o.intrinsics[1,1]*q[:,1]/safe+o.intrinsics[1,2]
            inside=(z>0)&(u>=0)&(u<1)&(v>=0)&(v<1)
            x=np.clip((u*w).astype(np.int64),0,w-1);y=np.clip((v*h).astype(np.int64),0,h-1)
            gap=z-o.depth[y,x]
            hit=inside&o.valid[y,x]&(o.labels[y,x]!=labels)&(o.labels[y,x]!=0)&(gap>scale*bias_relative)&(gap<scale*thickness_relative)
            hits+=hit
        support[ids]=np.minimum(hits/2,1)
    return support.reshape(h,w)

class DepthShellShadowBackend(ShadowEstimationBackend):
    name='depth-shell-support-v1'
    def __init__(self,steps=96):
        if not 16<=steps<=192:raise ValueError('Shadow ray steps must be 16..192')
        self.steps=steps
    def predict(self,o):
        shape=o.depth.shape
        if not np.isfinite(o.shading_scale) or o.shading_scale<=0:raise ValueError('Fixed shading scale must be positive')
        if not np.isfinite(o.light_confidence) or not 0<=o.light_confidence<=1:raise ValueError('Light confidence must be finite in [0,1]')
        for name in ('depth','normal','points','intrinsics','roughness','metallic','albedo','shading','error','uncertainty'):
            if not np.isfinite(getattr(o,name)).all():raise ValueError('Shadow input NaN/Inf: '+name)
        direct,ambient=coefficients(o.light);cosine=np.maximum(o.normal@-np.asarray(o.light['direction']),0)
        observed=o.shading/o.shading_scale
        # Subtract the same fixed ambient from the observation; near-zero direct remains unknown.
        denominator=cosine*(direct@LUMA)
        visibility=np.clip((observed@LUMA-ambient@LUMA)/np.maximum(denominator,.02),0,1).astype(np.float32)
        eligible=o.valid&(cosine>.15)&(denominator>.03)&(o.roughness>=.2)&(o.metallic<=.3)&(o.albedo.min(-1)>=.03)&~o.excluded
        eligible&=(abs(np.linalg.norm(o.normal,axis=-1)-1)<.005)&(o.error<.2)&(o.uncertainty<.3)
        geometry=geometry_support(o,eligible,self.steps)
        confidence=(eligible*geometry*np.exp(-o.error/.15)*np.exp(-o.uncertainty/.15)*np.clip((.95-visibility)/.5,0,1)*o.light_confidence).astype(np.float32)
        unknown=(confidence<.05).astype(np.uint32)
        candidate=(np.clip((.85-visibility)/.5,0,1)*(unknown==0)).astype(np.float32)
        parameters=dict(steps=self.steps,biasRelative=.01,thicknessRelative=.06,maxDistanceScale=2.,shadingScale=o.shading_scale)
        return dict(candidate=candidate,visibility=visibility,geometrySupport=geometry,confidence=confidence,unknown=unknown),parameters
