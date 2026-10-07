"""1500x1000 fine-detail appearance with 512x341 synthetic geometry, including an invalid island."""
import argparse
import json
import time
from pathlib import Path
from PIL import Image, ImageDraw
import numpy as np
from pipeline.runner import ReconstructionPipeline
from pipeline.geometry_backend import DummyGeometryBackend
from pipeline.lighting_backend import ManualLightingBackend


class PlaneWithHole(DummyGeometryBackend):
    name = 'ratio-test-plane-hole'

    def predict(self, image):
        p = super().predict(image)
        mask = p.valid_mask.copy();mask[image.height//3:image.height//2, image.width//3:image.width//2] = False
        from dataclasses import replace
        return replace(p, valid_mask=mask, confidence=np.where(mask,p.confidence,0).astype(np.float32),depth=np.where(mask,p.depth,0).astype(np.float32),
                       normal=np.where(mask[...,None],p.normal,0).astype(np.float32),
                       point_map=np.where(mask[...,None],p.point_map,0).astype(np.float32))


def create(output):
    output = Path(output)
    if output.exists():
        raise ValueError('Use a new fixture directory')
    output.parent.mkdir(parents=True,exist_ok=True)
    y,x=np.mgrid[:1000,:1500]
    rgb=np.stack([45+((x+y)%2)*55,40+((x//3+y//3)%2)*70,55+(x%101)],axis=-1).astype(np.uint8)
    image=Image.fromarray(rgb);draw=ImageDraw.Draw(image)
    for row in range(15,1000,25):
        draw.text((15,row),f'Native source 1500x1000 | detail, fine text, cracks | row {row}',fill=(175,165,155))
        draw.line([(1100,row),(1098,row+7),(1103,row+16),(1101,row+24)],fill=(9,12,15),width=1)
    path=output.with_suffix('.png');image.save(path)
    start=time.perf_counter()
    ReconstructionPipeline(geometry_backend=PlaneWithHole(),lighting_backend=ManualLightingBackend((0,0,1),(1,1,1),(.2,.2,.2))).run(path,output,max_size=512)
    report={'elapsedSeconds':time.perf_counter()-start,'source':[1500,1000],'analysis':[512,341],
            'models':'none; deterministic dummy plane, neutral material, manual light','input':str(path)}
    output.with_suffix('.timing.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(report)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output');create(p.parse_args().output)
