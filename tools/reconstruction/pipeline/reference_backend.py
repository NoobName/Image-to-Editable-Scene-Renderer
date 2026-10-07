"""Reference lighting proposals, never source pixel editing or per-pixel content matching."""
from dataclasses import dataclass
from copy import deepcopy
import numpy as np
from .lighting_backend import LightingInput, LightingEstimate, LUMA, coefficients
from .lighting_solver import RobustDirectionalAmbientBackend


def validate_light(light):
    if set(light)!={'direction','directColor','ambientColor','directIntensity','ambientIntensity'}:
        raise ValueError('Unsupported lighting parameter fields')
    for key in ('direction','directColor','ambientColor'):
        value=np.asarray(light[key],np.float64)
        if value.shape!=(3,) or not np.isfinite(value).all():
            raise ValueError('Light vector must be finite XYZ/RGB')
        if key=='direction':
            if abs(np.linalg.norm(value)-1)>1e-4:raise ValueError('Light travel direction must be normalized')
        elif np.any(value<0) or np.any(value>1):raise ValueError('Light colors must lie in [0,1]')
    for key in ('directIntensity','ambientIntensity'):
        if not np.isfinite(light[key]) or not 0<=light[key]<=64:raise ValueError('Relative intensity must lie in [0,64]')
    return deepcopy(light)


def parameters(record):
    return validate_light({'direction':record['direction'],'directColor':record['direct']['color'],
        'ambientColor':record['ambient']['color'],'directIntensity':record['direct']['intensity'],
        'ambientIntensity':record['ambient']['intensity']})


def rgb_coefficients(light):
    return tuple(np.array(light[k+'Color'],np.float64)*light[k+'Intensity'] for k in ('direct','ambient'))


@dataclass(frozen=True)
class ReferenceAnalysis:
    observation: LightingInput
    estimate: LightingEstimate
    provenance: dict


class ReferenceLightingMatcher:
    """Model-independent composition; the estimator and observation adapters are replaceable."""
    def __init__(self,lighting_backend=None):
        self.backend=lighting_backend or RobustDirectionalAmbientBackend()

    def analyze(self,observation,provenance=None):
        return ReferenceAnalysis(observation,self.backend.predict(observation),provenance or {})

    def propose(self,analysis,source_light,relation='different-content'):
        if relation not in ('same-scene','different-content'):raise ValueError('Unknown reference relation')
        source=validate_light(source_light);estimate=analysis.estimate;fit=estimate.fit
        old_d,old_b=rgb_coefficients(source);energy=float((old_d+old_b)@LUMA)
        reasons=list(fit['reasons']);supported=bool(fit['identifiable'] and fit['albedoSource']=='intrinsic' and fit['validFraction']>=.05)
        if fit['confidence']<.05:reasons.append('reference-fit-confidence-below-0.05');supported=False
        if energy<=1e-6:reasons.append('source-baseline-has-no-relative-energy');supported=False
        if analysis.provenance.get('fallbackReasons'):supported=False;reasons.extend(analysis.provenance['fallbackReasons'])
        target=deepcopy(source);scale=1.;chroma_clamped=False
        if supported:
            d,b=coefficients(estimate.source)
            # Transfer illumination chromaticity, NOT reference RGB means. A bounded chroma
            # range is still needed because intrinsic decomposition is approximate.
            adjusted=[]
            for value in (d,b):
                y=float(value@LUMA)
                chroma=value/max(y,1e-8);bounded=np.clip(chroma,.5,2.) if y>1e-8 else np.ones(3)
                chroma_clamped |= bool(np.max(np.abs(chroma-bounded))>1e-6 and y>1e-8)
                adjusted.append(bounded*y/max(float(bounded@LUMA),1e-8))
            d,b=adjusted;scale=energy/max(float((d+b)@LUMA),1e-8)
            d*=scale;b*=scale
            for key,value in (('direct',d),('ambient',b)):
                intensity=float(np.max(value));target[key+'Intensity']=min(intensity,64.)
                target[key+'Color']=(value/intensity if intensity>0 else np.ones(3)).tolist()
                if intensity>64:reasons.append('relative-intensity-clamped-to-64')
            target['direction']=estimate.source['direction']
        else:
            reasons.append('unsupported-lighting-fields-preserve-source-baseline; calibrate manually')
        validate_light(target)
        confidence=float(fit['confidence'])*(1. if relation=='same-scene' else .75) if supported else 0.
        unavailable=['shared-world-sun-position','absolute-radiance','HDR-environment-map','relative-exposure']
        if not supported:unavailable+=['main-light-direction','direct-ambient-ratio','illuminant-chromaticity']
        if chroma_clamped:reasons.append('illumination-chromaticity-bounded-to-0.5..2-relative-luminance')
        td,tb=rgb_coefficients(target)
        return {'target':target,'confidence':confidence,'canApply':supported,'directionSupported':supported,
            'relation':relation,'coordinateMapping':'camera-relative-identity-not-shared-world',
            'scaleRule':'preserve-source-direct-plus-ambient-luminance; exposure-fixed',
            'exposureEV':0.,'relativeExposureAvailable':False,'reasons':reasons,'unavailable':unavailable,
            'statistics':{'referenceToSourceScale':scale,'sourceEnergy':energy,'targetEnergy':float((td+tb)@LUMA),
                'directAmbientRatio':float(td@LUMA/max(float(tb@LUMA),1e-6)),
                'directionChangeDegrees':float(np.degrees(np.arccos(np.clip(np.dot(source['direction'],target['direction']),-1,1))))}}
