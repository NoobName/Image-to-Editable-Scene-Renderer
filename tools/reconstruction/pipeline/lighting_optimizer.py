"""Bounded target-only pattern search. Fixed observations, gauge, seed and sample set."""
from dataclasses import dataclass
from copy import deepcopy
import numpy as np
from .lighting_backend import LUMA,prepare_proxy
from .lighting_solver import huber
from .reference_backend import validate_light,rgb_coefficients


def evaluate_shading(normal,valid,light):
    """Match ImageShading.hlsl: unit normals within squared-length tolerance, camera travel."""
    validate_light(light);n=np.asarray(normal,np.float32);q=np.sum(n*n,axis=-1)
    mask=np.asarray(valid,bool)&np.isfinite(n).all(-1)&(np.abs(q-1)<.011)
    n=n/np.sqrt(np.maximum(q,1e-12))[...,None];d,b=rgb_coefficients(light)
    direction=np.asarray(light['direction'],np.float32);direction/=np.linalg.norm(direction)
    rgb=np.maximum(n@-direction,0)[...,None]*d.astype(np.float32)+b.astype(np.float32)
    return np.concatenate((np.where(mask[...,None],rgb,0),mask[...,None].astype(np.float32)),-1).astype(np.float32)


def vector(light):
    d=np.asarray(light['direction']);direct,ambient=rgb_coefficients(light)
    return np.r_[np.arctan2(d[0],d[2]),np.arcsin(np.clip(d[1],-1,1)),direct,ambient]


def light_from_vector(x):
    yaw,pitch=x[:2];d=[np.sin(yaw)*np.cos(pitch),np.sin(pitch),np.cos(yaw)*np.cos(pitch)]
    result={'direction':d}
    for key,v in (('direct',x[2:5]),('ambient',x[5:8])):
        intensity=float(np.max(v));result[key+'Intensity']=intensity;result[key+'Color']=(v/intensity if intensity>0 else np.ones(3)).tolist()
    return validate_light(result)


@dataclass
class OptimizationResult:
    target:dict
    report:dict
    mask:np.ndarray
    residual:np.ndarray
    initial:np.ndarray
    best:np.ndarray


def optimize(source,reference,analysis,initial,baseline,relation,*,registered=False,protection=None,
             iterations=100,seed=31,cancel=lambda:False,progress=lambda value:None,
             reference_proxy=None,reference_mask=None,reference_normal=None):
    if not 1<=iterations<=200:raise ValueError('Iterations must be in [1,200]')
    initial=validate_light(initial);baseline=validate_light(baseline);proposal=analysis['proposal'];initial_image=evaluate_shading(source.normal,source.valid,initial)
    mask=np.zeros(source.valid.shape,np.float32);residual=np.zeros_like(mask);history=[]
    report={'version':1,'status':'rejected','reason':'','relation':relation,'registered':bool(registered),'seed':seed,'maxIterations':iterations,
        'parameterization':'travel-yaw-pitch; nonnegative-directRGB-ambientRGB; fixed-Y(D+B)',
        'exposureDeltaBounds':[0,0],'maxDirectionChangeDegrees':90,'maxSamples':4096,'samples':0,'initialLoss':0.,'bestLoss':0.,
        'history':history,'approximation':'diffuse-unit-response only; excludes ratio, cast-shadow, specular and full-resolution display',
        'cpuHlslTolerance':5e-5}
    def finish(status,reason,target=initial,best=initial_image):
        report['status']=status;report['reason']=reason
        return OptimizationResult(deepcopy(target),report,mask,residual,initial_image,best)
    if cancel():return finish('cancelled','cancelled-before-evaluation')
    if not proposal['canApply']:return finish('rejected','reference-illumination-not-identifiable')
    if relation not in ('same-scene','different-content'):raise ValueError('Unknown optimization relation')
    src_proxy,sw,*_=prepare_proxy(source)
    sw=np.where(source.material_confidence>=.25,sw,0)
    if protection is not None:
        protection=np.asarray(protection);assert protection.shape==sw.shape
        if not np.isfinite(protection).all() or np.any((protection<0)|(protection>1)):raise ValueError('Invalid protection weights')
        sw*=1-protection
    desired=validate_light(proposal['target']);energy=float(sum(rgb_coefficients(baseline))@LUMA)
    if energy<=1e-6:return finish('rejected','source-baseline-zero-energy')
    angular=float(np.degrees(np.arccos(np.clip(np.dot(initial['direction'],desired['direction']),-1,1))))
    if angular>90:return finish('rejected','requested-direction-change-exceeds-90-degrees')
    if relation=='same-scene':
        if not registered:return finish('rejected','explicit-registered-pixel-alignment-required')
        if reference.valid.shape!=source.valid.shape:return finish('rejected','registered-resolution-mismatch')
        proxy,rw,*_=prepare_proxy(reference)
        if reference_proxy is not None:
            if reference_proxy.shape!=source.normal.shape or reference_mask.shape!=source.valid.shape or reference_normal.shape!=source.normal.shape:
                return finish('rejected','registered-saved-observation-resolution-mismatch')
            proxy=reference_proxy;rw=rw*(reference_mask!=0)
        agreement=(np.sum(source.normal*reference.normal,axis=-1)>.985)&(np.mean(np.abs(source.albedo-reference.albedo),axis=-1)<.08)
        sw*=rw*agreement*(reference.material_confidence>=.25)
        desired_pixels=proxy*proposal['statistics']['referenceToSourceScale']
    else:
        # No reference pixels participate in this objective; only fitted illumination statistics.
        desired_pixels=evaluate_shading(source.normal,source.valid,desired)[...,:3]
    mask[:]=sw.astype(np.float32);indices=np.flatnonzero(sw>0)
    if indices.size<64 or indices.size<sw.size*.05:return finish('rejected','insufficient-unprotected-confident-support')
    normals=source.normal.reshape(-1,3)[indices].astype(float);eigen=np.linalg.eigvalsh(np.cov(normals,rowvar=False))
    if eigen[0]<.002 or eigen[0]/max(eigen[-1],1e-9)<.01:return finish('rejected','degenerate-source-normal-distribution')
    rng=np.random.default_rng(seed)
    if indices.size>4096:indices=np.sort(rng.choice(indices,4096,replace=False))
    report['samples']=int(indices.size);report['sampleIndexHash']=__import__('hashlib').sha256(indices.astype('<u8').tobytes()).hexdigest()
    n=source.normal.reshape(-1,3)[indices].astype(float);n/=np.linalg.norm(n,axis=-1,keepdims=True)
    weights=sw.ravel()[indices];weights/=weights.sum();pixels=desired_pixels.reshape(-1,3)[indices]
    desired_x=vector(desired);initial_direction=np.asarray(initial['direction']);bound=min(64.,4*energy)
    def project(x):
        x=x.copy();x[0]=(x[0]+np.pi)%(2*np.pi)-np.pi;x[1]=np.clip(x[1],-np.pi/2+.001,np.pi/2-.001)
        x[2:]=np.clip(x[2:],0,bound);total=float((x[2:5]+x[5:8])@LUMA)
        if total<=1e-9:return None
        x[2:]*=energy/total
        if np.max(x[2:])>64:return None
        light=light_from_vector(x)
        if np.dot(initial_direction,light['direction'])<-1e-8:return None
        return x
    def loss(x):
        if x is None:return float('inf')
        light=light_from_vector(x);values=x[5:8]+np.maximum(n@-np.asarray(light['direction']),0)[:,None]*x[2:5]
        if relation=='same-scene':return float(np.sum(weights[:,None]*huber((values-pixels)/energy))/3)
        # Camera-relative direction/chroma/strength plus source-normal response distribution.
        direction=1-float(np.dot(light['direction'],desired['direction']))
        coefficient=float(np.mean(((x[2:]-desired_x[2:])/energy)**2))
        distribution=float(np.mean(((values.mean(0)-pixels.mean(0))/energy)**2)+np.mean(((values.std(0)-pixels.std(0))/energy)**2))
        return .2*direction+.4*coefficient+.4*distribution
    best_x=project(vector(initial))
    if best_x is None:return finish('rejected','initial-parameters-outside-feasible-gauge')
    best_loss=loss(best_x);report['initialLoss']=best_loss;report['bestLoss']=best_loss
    steps=np.r_[np.deg2rad([10.,10.]),np.full(6,.15*energy)]
    for iteration in range(iterations):
        if cancel():return finish('cancelled','cancelled; original-target-retained')
        improved=False;candidate_loss=best_loss
        for axis in range(8):
            for sign in (-1,1):
                trial=best_x.copy();trial[axis]+=sign*steps[axis];trial=project(trial);value=loss(trial);candidate_loss=value if np.isfinite(value) else candidate_loss
                if value<best_loss-1e-12:best_x=trial;best_loss=value;improved=True
        if not improved:steps*=.5
        history.append({'iteration':iteration,'bestLoss':float(best_loss),'candidateLoss':float(candidate_loss),'parameters':best_x.tolist(),'stepMaximum':float(steps.max())})
        progress(history[-1]);report['bestLoss']=float(best_loss)
        if steps.max()<1e-5:break
    target=light_from_vector(best_x);best=evaluate_shading(source.normal,source.valid,target)
    residual[:]=np.mean(np.abs(best[...,:3]-desired_pixels),axis=-1)* (sw>0)
    if not np.isfinite(best_loss) or best_loss>=report['initialLoss']-max(1e-8,report['initialLoss']*.001):return finish('no-improvement','no-significant-fixed-loss-improvement')
    # This is an independent illumination constraint, not the trivial source identity ratio.
    if relation=='same-scene' and best_loss>.02:return finish('rejected','registered-reference-not-explained-by-fixed-diffuse-observations')
    return finish('improved','best-feasible-candidate; DX12 verification required before Apply',target,best)
