"""Intrinsic-assisted Lambert fit. A single scalar gauge, no spatial/color gain field."""
from dataclasses import replace
import numpy as np
from .lighting_backend import LUMA, prepare_proxy
from .lighting_solver import RobustDirectionalAmbientBackend
from .material_estimation_backend import srgb_to_linear


def diffuse_support(o, maps):
    """Conservative evidence weight, not a calibrated probability or specular classifier."""
    a, s = maps['albedo'][..., :3], maps['shading'][..., :3]
    residual = maps['residual'][..., :3] if 'residual' in maps else np.zeros_like(a)
    linear = srgb_to_linear(o.rgb.astype(np.float32)/255)
    error = np.sqrt(np.mean((linear-a*s-residual)**2, -1))
    uncertainty = maps['uncertainty'][..., 1]
    fraction = np.max(np.abs(residual), -1)/(np.max(a*s+np.abs(residual), -1)+.02)
    support = np.exp(-error/.15)*np.exp(-uncertainty/.15)*np.clip(1-fraction/.5, 0, 1)
    support *= (maps['validity'] != 0) & (np.min(a, -1) >= .03)
    return support, fraction, error


class IntrinsicAssistedBackend(RobustDirectionalAmbientBackend):
    name = 'intrinsic-assisted'

    def __init__(self, maps=None, metadata=None, digest=''):
        self.maps, self.metadata, self.digest = maps, metadata, digest

    def bind_package(self, root, appearance):
        from intrinsic_contract import load_intrinsic, read_arrays
        from lighting_contract import sha256
        self.metadata = load_intrinsic(root, appearance)
        self.maps = read_arrays(root, self.metadata) if self.metadata else None
        self.digest = sha256(root/'intrinsic/intrinsic.json') if self.metadata else ''

    def predict(self, observation):
        baseline = RobustDirectionalAmbientBackend().predict(observation)
        candidate = None; reason = 'missing-intrinsic'; scale = 1.; comparison = []
        # A Proxy reuses I/A and supplies no independent observation; never promote it to an improvement.
        if self.maps is not None and self.metadata['provenance']['provenance'] in ('estimated', 'synthetic'):
            o = replace(observation, albedo=self.maps['albedo'][..., :3], albedo_source='intrinsic')
            _, weights, flags, _, exclusions = prepare_proxy(o)
            support, _, _ = diffuse_support(o, self.maps)
            weights *= support; weights[support < .1] = 0
            flags[weights == 0] |= 1 << 6
            observed = self.maps['shading'][..., :3].astype(np.float64).copy()
            mask = weights > 0
            scale = max(float(np.median(observed[mask] @ LUMA)), 1e-6) if mask.any() else 1.
            observed /= scale; observed[~mask] = 0
            candidate = self.fit_prepared(o, (observed, weights, flags, scale, exclusions))
            reason = 'accepted' if candidate.fit['identifiable'] else 'degenerate-intrinsic-fit'
            # Compare both predictions against the SAME independent shading gauge/support.
            # Separate own-proxy RMSE values are also retained, but are not directly comparable.
            # Both observations have their own median Y fixed at one. Native checkpoint and I/A
            # scales are ambiguous, so comparing their unnormalized magnitudes would be misleading.
            b = baseline.old_shading
            c = candidate.old_shading
            for label in np.unique(o.labels):
                m = (o.labels == label) & mask
                eb = float(np.sqrt(np.mean((b[m]-observed[m])**2))) if m.any() else 0.
                ec = float(np.sqrt(np.mean((c[m]-observed[m])**2))) if m.any() else 0.
                comparison.append({'label': int(label), 'pixels': int(m.sum()), 'baselineRMSE': eb,
                    'assistedRMSE': ec, 'classification': 'unavailable' if not m.any() else
                    'tie' if abs(eb-ec) <= .01 else 'improved' if ec < eb else 'degraded'})
        elif self.maps is not None:
            reason = 'proxy-is-not-independent-intrinsic'
        selected = candidate is not None and candidate.fit['identifiable']
        result = candidate if selected else baseline
        def summary(estimate):
            f = estimate.fit
            return {k: f[k] for k in ('afterRMSE', 'normalization', 'identifiable', 'confidence', 'directionGap')}
        evidence = {'version': 1, 'selected': selected, 'reason': reason, 'intrinsicSha256': self.digest,
            'baselineSource': baseline.source, 'candidateSource': candidate.source if candidate else baseline.source,
            'baselineFit': summary(baseline), 'candidateFit': summary(candidate if candidate else baseline),
            'calibrationScale': scale, 'colorGain': [1., 1., 1.], 'regions': comparison}
        # Export schema marks the requested backend even on fallback; provenance still describes the selected fit.
        result.fit['backend'] = self.name
        return replace(result, assistance=evidence)
