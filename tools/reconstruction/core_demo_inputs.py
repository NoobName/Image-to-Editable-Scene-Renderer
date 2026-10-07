"""Reproducible project-authored diagnostics, not photographs or learned predictions."""
import argparse
import hashlib
from pathlib import Path
from scene_package import write_json_atomic
from reference_examples import create as reference
from cast_shadow_examples import create as shadow
from specular_examples import create as specular
from ratio_examples import create as detail
from stability_examples import create as stability


def create(output):
    output.mkdir(parents=True, exist_ok=False)
    reference(output/'reference')
    shadow(output/'shadow')
    specular(output/'specular')
    detail(output/'detail')
    stability(output/'stability')
    images = []
    for path in sorted(output.rglob('*.png')):
        if path.parent == output or path.parent.name in ('reference', 'shadow', 'specular'):
            images.append(dict(path=path.relative_to(output).as_posix(), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                               source='project procedural generator; no external photo/model',
                               rights='generated test asset; no third-party photo rights asserted'))
    write_json_atomic(output/'sources.json', dict(version=1, images=images,
        roles={'detail':'1500x1000 fine text with synthetic mesh hole', 'reference':'known Lambert lights / multi-material and degenerate plane',
               'specular/sphere':'analytic GGX, NOT physical glass transmission', 'shadow/black':'black signal cannot recover texture',
               'shadow/missing':'baked shadow without supported visible occluder', 'stability':'saturated/dark/high-metal/low-roughness/invalid stress tiles'}))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    create(p.parse_args().output)
