"""Print precision evidence for float32 GGX peaks before choosing supported-domain tolerances."""
import json
from pathlib import Path
import numpy as np
from runtime_dds import read_dds
from specular_reference import ggx
root=Path('generated/scene24-final-indoor');capture=Path('generated/prompt25-diffuse-anchor-shading')
r=json.loads((capture/'readback.json').read_text(encoding='utf-8'));meta=json.loads((root/'analysis/analysis.json').read_text(encoding='utf-8'))
def load(k):return read_dds(root/meta['maps'][k]['path'])[1].astype(float)
n,p,a,rough,metal=load('normal'),load('position'),load('albedo'),load('roughness'),load('metallic');light=r['source']
expected=ggx(n[...,:3],p[...,:3],a[...,:3],rough,metal,light['direction'],np.array(light['directColor'])*light['directIntensity'])*r['composition']['specular']['scale']
w,h=r['specularOld']['size'];actual=np.fromfile(capture/'specularOld.bin',dtype='<f4').reshape(h,w,4)[...,:3]
error=abs(actual-expected);pixel=np.unravel_index(np.argmax(error),error.shape);support=(rough>=.2)&(rough<=.8)&(metal<=.3)
print(dict(pixel=pixel,roughness=rough[pixel[:2]],metallic=metal[pixel[:2]],expected=expected[pixel],actual=actual[pixel],error=error[pixel],supportedMaxError=error[support].max()))
