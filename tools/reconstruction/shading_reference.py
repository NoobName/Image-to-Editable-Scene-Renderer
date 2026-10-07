"""Independent float64 reference for the image-domain unit-reflectance response.

No pi division: fitted direct/ambient coefficients already absorb pi and exposure.
Geometry normal is LH camera, direction is light travel, and alpha is validity.
"""
import numpy as np


def evaluate(normal, validity, light):
    n = np.asarray(normal, dtype=np.float64)[..., :3]
    d = np.asarray(light['direction'], dtype=np.float64)
    lengths = np.linalg.norm(n, axis=-1)
    valid = (np.asarray(validity) != 0) & np.isfinite(n).all(-1) & (np.abs(lengths * lengths - 1) < .011)
    result = np.zeros((*n.shape[:-1], 4), dtype=np.float64)
    if not np.isfinite(d).all() or np.linalg.norm(d) < 1e-6:
        return result
    cosine = np.maximum(np.einsum('...c,c->...', n / np.maximum(lengths[..., None], 1e-12), -d / np.linalg.norm(d)), 0)
    rgb = cosine[..., None] * np.asarray(light['directColor']) * light['directIntensity'] + np.asarray(light['ambientColor']) * light['ambientIntensity']
    result[..., :3] = np.where(valid[..., None], rgb, 0)
    result[..., 3] = valid
    return result
