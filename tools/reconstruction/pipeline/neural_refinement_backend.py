"""Optional offline refinement boundary. Concrete models are isolated in adapters.

This contract and its guards are usable independently of candidate research. A guarded
physics return is explicitly reported as bypass/fallback, never a successful inference.
"""
from abc import ABC, abstractmethod
from dataclasses import dataclass
import re
import numpy as np
from .reference_backend import validate_light


@dataclass(frozen=True)
class RefinementInput:
    original_rgb: np.ndarray  # immutable canonical RGB8 sRGB, native H x W x 3
    physics_rgb: np.ndarray   # native display-referred linear RGB32; not HDR radiance
    source_lighting: dict     # normalized LH camera light-travel direction
    target_lighting: dict     # global gain is a separate, fingerprinted scalar below
    guidance: dict            # analysis-resolution depth/normal/old/new + explicit validity
    protection: np.ndarray    # native float32 [0,1]; 1 means retain physics exactly
    source_sha256: str
    physics_sha256: str
    scale_type: str
    target_gain: float = 1.0


@dataclass(frozen=True)
class RefinementParameters:
    strength: float = 0.0
    seed: int = 34
    max_side: int = 256

    def validate(self):
        if not np.isfinite(self.strength) or not 0 <= self.strength <= 1:
            raise ValueError('Refinement strength must be finite in [0,1]')
        if isinstance(self.seed, bool) or not isinstance(self.seed, int) or not 0 <= self.seed < 2**32:
            raise ValueError('Refinement seed must be an unsigned 32-bit integer')
        if self.max_side not in (256, 512):
            raise ValueError('Refinement budget supports explicit 256 or 512 processing side')


@dataclass(frozen=True)
class RefinementCandidate:
    rgb: np.ndarray
    provenance: dict


class NeuralRefinementBackend(ABC):
    """Adapters alone own model code. Renderer and this contract never import Torch."""
    @abstractmethod
    def predict(self, observation: RefinementInput, parameters: RefinementParameters,
                cancelled) -> RefinementCandidate:
        """Must retain exact model/code revisions, hashes, guidance semantics and seed."""
        raise NotImplementedError


def validate_input(value):
    src, physics = value.original_rgb, value.physics_rgb
    if src.dtype != np.uint8 or src.ndim != 3 or src.shape[-1] != 3:
        raise ValueError('Canonical source must be RGB8')
    h, w = src.shape[:2]
    if min(h, w) < 1 or max(h, w) > 8192 or h*w > 8*1024*1024:
        raise ValueError('Refinement native image budget exceeded; never silently resize source')
    for name, array, shape in [('physics', physics, src.shape), ('protection', value.protection, (h,w))]:
        if array.dtype != np.float32 or array.shape != shape or not np.isfinite(array).all():
            raise ValueError(f'{name} dimensions/type/finite contract violated')
        if np.any(array < 0) or (name == 'protection' and np.any(array > 1)):
            raise ValueError(f'{name} range invalid')
    for digest in (value.source_sha256, value.physics_sha256):
        if not re.fullmatch('[a-f0-9]{64}', digest):
            raise ValueError('Input hashes must be lowercase SHA256, verified by the file loader')
    validate_light(value.source_lighting); validate_light(value.target_lighting)
    if not np.isfinite(value.target_gain) or not 0 <= value.target_gain <= 64:
        raise ValueError('Target global gain must be finite in [0,64]')
    if value.scale_type not in ('metric', 'relative', 'synthetic', 'unknown'):
        raise ValueError('Unknown geometry scale type')
    expected = {'depth', 'normal', 'oldShading', 'newShading', 'validity', 'confidence', 'relightingConfidence'}
    if set(value.guidance) != expected:
        raise ValueError('Require explicit depth/normal/old/new/validity/confidence guidance')
    valid = value.guidance['validity']
    if valid.dtype != np.bool_ or valid.ndim != 2 or min(valid.shape) < 1 or max(valid.shape) > 2048:
        raise ValueError('Analysis validity must be bounded boolean H x W')
    for name in expected - {'validity'}:
        data = value.guidance[name]
        shape = (h,w) if name=='relightingConfidence' else ((*valid.shape,3) if name in ('normal','oldShading','newShading') else valid.shape)
        if data.dtype != np.float32 or data.shape != shape or not np.isfinite(data).all():
            raise ValueError(f'Analysis {name} has invalid dimensions/type/values')
        if name != 'normal' and np.any(data < 0):
            raise ValueError(f'Negative {name}')
    if np.any(value.guidance['depth'][valid] <= 0):
        raise ValueError('Valid camera-Z depth must be positive')
    if not np.allclose(np.linalg.norm(value.guidance['normal'][valid],axis=-1),1,atol=1e-4):
        raise ValueError('Valid normal must be unit LH camera normal')
    if np.any(value.guidance['confidence'] > 1) or np.any(value.guidance['relightingConfidence'] > 1):
        raise ValueError('Confidence must be [0,1]')


def run_guarded(observation, parameters, backend=None, cancelled=lambda: False):
    """Boundary guards only: no registered backend is an explicit physics fallback.

    Invalid input is rejected, not interpreted as a model failure. Model errors leave
    the caller's original/physics arrays untouched. Quality gates belong in an adapter
    and require real model validation before integration into the application.
    """
    validate_input(observation); parameters.validate()
    reason = bypass_reason(observation, parameters)
    if reason or cancelled():
        return RefinementCandidate(observation.physics_rgb.copy(),
            {'status': 'bypassed', 'reason': reason or 'cancelled', 'neuralInference': False})
    if backend is None:
        return RefinementCandidate(observation.physics_rgb.copy(),
            {'status': 'unavailable', 'reason': 'no-validated-pretrained-adapter', 'neuralInference': False})
    try:
        candidate = backend.predict(observation, parameters, cancelled)
        if cancelled():
            raise RuntimeError('cancelled')
        if candidate.rgb.shape != observation.physics_rgb.shape or candidate.rgb.dtype != np.float32 or not np.isfinite(candidate.rgb).all() or np.any(candidate.rgb < 0):
            raise ValueError('Nonfinite or malformed neural candidate')
        required = {'modelRevision','codeRevision','weightSha256','seed','guidance'}
        if not required <= candidate.provenance.keys() or candidate.provenance['seed'] != parameters.seed:
            raise ValueError('Incomplete model provenance')
        weight = parameters.strength*(1-observation.protection[...,None])
        blended = observation.physics_rgb + weight*(candidate.rgb-observation.physics_rgb)
        # Avoid even roundoff in protected pixels; physics is the immutable fallback.
        blended = np.where(weight == 0, observation.physics_rgb, blended).astype(np.float32)
        return RefinementCandidate(blended, {**candidate.provenance,'status':'candidate','neuralInference':True})
    except Exception as error:
        return RefinementCandidate(observation.physics_rgb.copy(),
            {'status':'fallback','reason':f'{type(error).__name__}: {error}','neuralInference':False})


def bypass_reason(observation, parameters):
    if parameters.strength == 0:return 'strength-zero'
    if observation.source_lighting == observation.target_lighting and observation.target_gain == 1:return 'lighting-unchanged'
    rgb=observation.original_rgb.astype(np.float32)/255
    original=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4).astype(np.float32)
    if np.array_equal(original,observation.physics_rgb):return 'physics-identity'
    return None
