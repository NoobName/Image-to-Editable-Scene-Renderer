"""Publish fitting evidence, never source-times-shading or a relit image."""
from copy import deepcopy
import numpy as np
from PIL import Image, ImageDraw
try:
    from ..runtime_dds import write_dds
    from ..scene_package import write_json_atomic
    from ..lighting_contract import INPUTS, KEYS, sha256
except ImportError:
    from runtime_dds import write_dds
    from scene_package import write_json_atomic
    from lighting_contract import INPUTS, KEYS, sha256
from .material_estimation_backend import linear_to_srgb, quantize


def write_lighting_assets(root, image, appearance, estimate):
    directory = root / 'lighting'; directory.mkdir(exist_ok=True)
    maps = {}
    for i, (key, a) in enumerate(zip(KEYS, (estimate.proxy, estimate.old_shading, estimate.residual, estimate.fit_mask))):
        fmt = 'RGBA32_FLOAT' if i < 2 else 'R32_FLOAT' if i == 2 else 'R32_UINT'
        if i < 2:
            a = np.concatenate((a, np.zeros((*a.shape[:2], 1), np.float32)), -1)
        path = f'lighting/{key}.dds'; write_dds(root / path, a, fmt)
        maps[key] = {'path': path, 'sha256': sha256(root / path), 'format': fmt}
    np.savez_compressed(directory / 'fit_evidence.npz', weights=estimate.weights, rejection_flags=estimate.rejection_flags)
    data = {'version': 1, 'sourceId': appearance['sourceId'], 'sourceSha256': appearance['sourceImage']['sha256'],
        'analysisSize': [image.width, image.height], 'directionConvention': 'light-travel-lh-camera',
        'scaleConvention': 'median-luminance-proxy-fixed-exposure-albedo',
        'sourceLighting': estimate.source, 'targetLighting': deepcopy(estimate.source),
        'inputs': [{'path': p, 'sha256': sha256(root / p)} for p in INPUTS], 'fit': estimate.fit, 'maps': maps}
    if estimate.assistance is not None:
        data['assistance'] = estimate.assistance
    write_json_atomic(directory / 'lighting.json', data)
    write_lighting_diagnostic(directory, estimate)
    return data


def write_lighting_diagnostic(directory, estimate):
    # Fixed display scales make before/after evidence comparable; the DDS retains unbounded float values.
    residual = np.minimum(estimate.residual, 1)
    previews = (quantize(linear_to_srgb(estimate.proxy/4)), quantize(linear_to_srgb(estimate.old_shading/4)),
                quantize(np.stack((residual, .2*residual, 1-residual), -1))*estimate.fit_mask[..., None].astype(np.uint8),
                np.repeat((estimate.fit_mask*255).astype(np.uint8)[..., None], 3, -1))
    canvas = Image.new('RGB', (800, 520), '#20252b'); draw = ImageDraw.Draw(canvas)
    for i, (name, p) in enumerate(zip(('Shading Proxy /4', 'Fitted Old Shading /4', 'Residual Y [0,1]', 'Fit Mask'), previews)):
        picture = Image.fromarray(p); picture.thumbnail((390, 195))
        x, y = (i % 2)*400, (i//2)*230
        canvas.paste(picture, (x, y+24)); draw.text((x+5, y+5), name, fill='white')
        Image.fromarray(p).save(directory / (KEYS[i]+'.png'))
    source = estimate.source; d = source['direction']; fit = estimate.fit
    # XY arrow is TO-light; Z is printed because a 2D arrow cannot encode the full direction.
    # Camera +Y points up; image +Y points down. TO-light = -travel, so screen deltaY = +travelY.
    draw.line((40, 485, 40-25*d[0], 485+25*d[1]), fill='yellow', width=3)
    draw.text((5, 455), 'TO-light XY', fill='yellow')
    draw.text((80, 465), f"Travel XYZ: {d[0]:.3f}, {d[1]:.3f}, {d[2]:.3f}   confidence={fit['confidence']:.3f}", fill='white')
    draw.text((80, 485), f"Relative direct={source['direct']['intensity']:.3f} ambient={source['ambient']['intensity']:.3f} median={fit['normalization']:.3f}", fill='white')
    draw.text((80, 505), f"{fit['status']} RMSE ambient {fit['beforeRMSE']:.4f} -> fitted {fit['afterRMSE']:.4f}; exposure=0, albedo gain=1", fill='white')
    canvas.save(directory / 'diagnostic.png')
