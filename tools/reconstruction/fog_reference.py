"""Float64 CPU reference for additional image haze, independent of DirectX implementation."""
import numpy as np


def sample(distance, labels, width, height):
    ah, aw = distance.shape[:2]
    y, x = np.mgrid[:height, :width]
    px, py = (x+.5)*aw/width-.5, (y+.5)*ah/height-.5
    nx, ny = np.clip(np.floor(px+.5).astype(int), 0, aw-1), np.clip(np.floor(py+.5).astype(int), 0, ah-1)
    center = distance[ny, nx].astype(np.float64)
    fx, fy = px-np.floor(px), py-np.floor(py)
    bx, by = np.floor(px).astype(int), np.floor(py).astype(int)
    result = np.zeros_like(center); good = center[..., 3] > 0
    for oy in range(2):
        for ox in range(2):
            qx, qy = np.clip(bx+ox, 0, aw-1), np.clip(by+oy, 0, ah-1)
            v = distance[qy, qx]
            good &= (v[..., 3] > 0)&(labels[qy, qx] == labels[ny, nx])&(abs(v[..., 2]-center[..., 2]) <= .05*np.minimum(v[..., 2], center[..., 2]))
            result += v*((fx if ox else 1-fx)*(fy if oy else 1-fy))[..., None]
    return np.where((center[..., 3] > 0)[..., None], np.where(good[..., None], result, center), 0)


def evaluate(baseline, samples, density, airlight, protection=0, active=True):
    confidence = np.clip(samples[..., 1], 0, 1)*(1-np.clip(protection, 0, 1)) if active else np.zeros(samples.shape[:2])
    t = np.exp(-np.minimum(density*samples[..., 0], 80))
    effective = 1-confidence*(1-t)
    output = baseline[..., :3]*effective[..., None]+np.asarray(airlight)*(1-effective[..., None])
    return output, effective, confidence
