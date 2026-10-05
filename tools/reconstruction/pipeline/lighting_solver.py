"""Deterministic spherical search + Huber IRLS with a two-variable nonnegative active set."""
import numpy as np
from .lighting_backend import LightingEstimationBackend, LightingEstimate, LUMA, prepare_proxy, light_record, coefficients

HUBER_DELTA = .08


def huber(residual):
    a = np.abs(residual)
    return np.where(a <= HUBER_DELTA, .5*a*a, HUBER_DELTA*(a-.5*HUBER_DELTA))


def nonnegative_pair(x, y, weights):
    """Solve y ~= a*x+b for many directions, testing the interior and both nonnegative boundaries."""
    x = np.asarray(x, np.float64)
    if x.ndim == 1:
        x = x[:, None]
    y = np.asarray(y, np.float64)[:, None]
    base = np.asarray(weights, np.float64)[:, None]
    weight = np.broadcast_to(base, x.shape).copy()
    for _ in range(7):
        s = weight.sum(0); sx = (weight*x).sum(0); sy = (weight*y).sum(0)
        sxx = (weight*x*x).sum(0); sxy = (weight*x*y).sum(0)
        det = s*sxx-sx*sx
        a = np.divide(s*sxy-sx*sy, det, out=np.zeros_like(det), where=det > 1e-10)
        b = np.divide(sy-a*sx, s, out=np.zeros_like(s), where=s > 0)
        candidates = ((a, b), (np.maximum(sxy/np.maximum(sxx, 1e-12), 0), np.zeros_like(s)),
                      (np.zeros_like(s), np.maximum(sy/np.maximum(s, 1e-12), 0)))
        errors = np.stack([np.where((aa >= 0)&(bb >= 0), (weight*(aa*x+bb-y)**2).sum(0), np.inf) for aa, bb in candidates])
        choice = errors.argmin(0)
        a = np.choose(choice, [c[0] for c in candidates]); b = np.choose(choice, [c[1] for c in candidates])
        residual = a*x+b-y
        weight = base*np.minimum(1, HUBER_DELTA/np.maximum(np.abs(residual), 1e-12))
    loss = (base*huber(a*x+b-y)).sum(0)/max(float(base.sum()), 1e-12)
    return a, b, loss


def sphere_directions(count=512):
    i = np.arange(count); z = 1-2*(i+.5)/count; angle = i*np.pi*(3-np.sqrt(5))
    return np.stack((np.sqrt(1-z*z)*np.cos(angle), np.sqrt(1-z*z)*np.sin(angle), z), -1)


def finish_estimate(o, source, proxy, weight, flags, normalization, exclusions, fit):
    mask = weight > 0
    direct, ambient = coefficients(source)
    old = np.maximum(o.normal @ -np.array(source["direction"]), 0)[..., None]*direct+ambient
    old[(flags & 1) != 0] = 0
    error = np.abs((proxy-old) @ LUMA); error[~mask] = 0
    luminance = proxy @ LUMA
    denominator = max(float(weight.sum()), 1e-12)
    mean = float((weight*luminance).sum()/denominator)
    before = float(np.sqrt((weight*(luminance-mean)**2).sum()/denominator))
    after = float(np.sqrt((weight*error**2).sum()/denominator))
    regions = []
    for label in np.unique(o.labels):
        r = o.labels == label; supported = r & mask
        regions.append({"label": int(label), "pixels": int(r.sum()), "supported": int(supported.sum()),
                        "meanResidual": float(error[supported].mean()) if supported.any() else 0.})
    fit = {**fit, "validPixels": int(mask.sum()), "totalPixels": int(mask.size), "validFraction": float(mask.mean()),
           "beforeRMSE": before, "afterRMSE": after, "robustLoss": float((weight*huber(error)).sum()/denominator),
           "normalization": normalization, "exposureEV": 0, "albedoGain": 1, "huberDelta": HUBER_DELTA,
           "excludedCounts": exclusions, "regions": regions, "inputProvenance": o.provenance,
           "albedoSource": o.albedo_source}
    return LightingEstimate(source, fit, proxy.astype(np.float32), old.astype(np.float32), error.astype(np.float32),
                            mask.astype(np.uint32), weight.astype(np.float32), flags)


class RobustDirectionalAmbientBackend(LightingEstimationBackend):
    name = "robust-directional-ambient"

    def predict(self, observation):
        o = observation
        proxy, weight, flags, normalization, exclusions = prepare_proxy(o)
        indices = np.flatnonzero(weight)
        # Bounded deterministic spatial subsampling makes offline fitting independent of BLAS threading.
        indices = indices[np.linspace(0, len(indices)-1, min(len(indices), 4096), dtype=int)] if len(indices) else indices
        n = o.normal.reshape(-1, 3)[indices].astype(np.float64)
        y_rgb = proxy.reshape(-1, 3)[indices]; y = y_rgb @ LUMA; w = weight.reshape(-1)[indices]
        reasons = []; eigen = np.zeros(3); gap = 0.
        l = np.array([0., 0., -1.]); direct = np.zeros(3); ambient = np.zeros(3)
        if len(indices) < 64 or np.mean(weight > 0) < .05:
            reasons.append("insufficient-valid-support")
        if len(indices) >= 3:
            centered = n-np.average(n, axis=0, weights=w)
            eigen = np.linalg.eigvalsh((centered*w[:, None]).T@centered/w.sum())
        if eigen[0] < .002 or eigen[0]/max(eigen[-1], 1e-12) < .01:
            reasons.append("normal-diversity-rank-deficient")
        if len(indices):
            ambient = np.average(y_rgb, axis=0, weights=w)
        if not reasons:
            directions = sphere_directions()
            aa, bb, losses = nonnegative_pair(np.maximum(n@directions.T, 0), y, w)
            best = int(losses.argmin()); l = directions[best]
            far = directions@l < np.cos(np.deg2rad(20))
            gap = float(max(0., (losses[far].min()-losses[best])/max(losses[far].min(), 1e-6)))
            for degrees in (8, 4, 2, 1, .5, .25):
                axis = np.array([1., 0., 0.]) if abs(l[0]) < .9 else np.array([0., 1., 0.])
                t = np.cross(l, axis); t /= np.linalg.norm(t); b = np.cross(l, t)
                angles = np.arange(8)*np.pi/4
                ring = np.cos(np.deg2rad(degrees))*l+np.sin(np.deg2rad(degrees))*(np.cos(angles)[:, None]*t+np.sin(angles)[:, None]*b)
                options = np.vstack((l, ring)); _, _, local = nonnegative_pair(np.maximum(n@options.T, 0), y, w)
                l = options[local.argmin()]
            x = np.maximum(n@l, 0)
            for channel in range(3):
                a, b, _ = nonnegative_pair(x, y_rgb[:, channel], w)
                direct[channel], ambient[channel] = a[0], b[0]
            if direct@LUMA < .05:
                reasons.append("directional-signal-too-small")
            if gap < .01:
                reasons.append("ambiguous-direction-profile")
        if np.any(direct > 64) or np.any(ambient > 64):
            reasons.append("relative-coefficient-out-of-range")
        identifiable = not reasons
        if o.albedo_source != "intrinsic":
            reasons.append("albedo-is-not-intrinsic")
        source = light_record(l, np.clip(direct, 0, 64), np.clip(ambient, 0, 64), "estimated-baseline" if identifiable and o.albedo_source == "intrinsic" else "unreliable-baseline")
        preliminary = finish_estimate(o, source, proxy, weight, flags, normalization, exclusions,
            {"backend": self.name, "status": "fitted", "confidence": 0., "identifiable": identifiable,
             "reasons": reasons, "normalEigenvalues": eigen.tolist(), "directionGap": gap})
        fit = preliminary.fit
        improvement = max(0., 1-fit["afterRMSE"]/max(fit["beforeRMSE"], 1e-6))
        confidence = float(min(1., fit["validFraction"]/.4)*improvement*np.exp(-fit["afterRMSE"])) if identifiable else 0.
        if o.albedo_source != "intrinsic":
            confidence = min(confidence, .1)
            fit["identifiable"] = False
        fit["confidence"] = confidence
        fit["status"] = "degenerate" if not identifiable else "low-confidence" if confidence < .5 else "fitted"
        return preliminary
