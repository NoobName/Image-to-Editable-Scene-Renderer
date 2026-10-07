"""Offline full-pipeline benchmark. New output only; cached weights are never downloaded.

First/repeat refer to process and OS-cache warmth, NOT a resident-model service:
the production adapters deliberately release models between stages on an 8 GB GPU.
"""
import argparse
import json
import time
from pathlib import Path
from pipeline.runner import ReconstructionPipeline
from pipeline.geometry_backend import DummyGeometryBackend
from pipeline.material_estimation_backend import NeutralMaterialBackend
from pipeline.segmentation_backend import DummySegmentationBackend
from pipeline.intrinsic_backend import ProxyIntrinsicBackend
from pipeline.diffuse_backend import IntrinsicAssistedBackend
from estimate_intrinsic import export_intrinsic
from export_analysis import export_saved
from estimate_shadows import export_shadows
from scene_package import write_json_atomic


class MeasuredBackend:
    def __init__(self, backend, rows, cuda):
        self.backend, self.rows, self.cuda = backend, rows, cuda
        self.name = backend.name

    def predict(self, *args):
        if self.cuda:
            import torch
            torch.cuda.synchronize()
            torch.cuda.reset_peak_memory_stats()
        start = time.perf_counter()
        value = self.backend.predict(*args)
        if self.cuda:
            torch.cuda.synchronize()
        row = dict(backend=self.name, seconds=time.perf_counter()-start,
                   metadata=getattr(value, 'metadata', {}))
        if self.cuda:
            row.update(peakAllocatedBytes=torch.cuda.max_memory_allocated(),
                       peakReservedBytes=torch.cuda.max_memory_reserved())
        self.rows.append(row)
        return value

    def release(self):
        self.backend.release()


def run(input_path, output, sizes, real):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = dict(version=1, realModels=real, runs=[],
                  timingScope='synchronized predict incl lazy load/decode; stage events also include validation/release; no download',
                  warmth='first in process then repeat with warm Python/OS caches; adapters reload/release models, not resident inference')
    if real:
        import torch
        if not torch.cuda.is_available():
            raise RuntimeError('Real profiling requires CUDA; use Dummy explicitly otherwise')
        report['device'] = torch.cuda.get_device_name()
        report['torch'] = torch.__version__
    for index, size in enumerate(sizes):
        directory = output / f'run-{index}-{size}'
        directory.mkdir()
        rows, events, starts = [], [], {}
        def event(stage, state, message):
            if state == 'running':
                starts[stage] = time.perf_counter()
            elif state == 'complete':
                events.append(dict(stage=stage, seconds=time.perf_counter()-starts[stage], message=message))
        if real:
            from pipeline.adapters.moge import MoGeGeometryBackend
            from pipeline.adapters.sam2 import Sam2SegmentationBackend
            from pipeline.adapters.marigold import MarigoldMaterialBackend
            from pipeline.adapters.marigold_intrinsic import MarigoldIntrinsicBackend
            geometry = MoGeGeometryBackend('cuda', offline=True, resolution_level=0)
            segmentation = Sam2SegmentationBackend('cuda', offline=True, points_per_side=8)
            material = MarigoldMaterialBackend('cuda', True, size, 4, 3, 13)
            intrinsic = MarigoldIntrinsicBackend('cuda', True, size, 4, 3, 23)
        else:
            geometry, segmentation, material, intrinsic = DummyGeometryBackend(), DummySegmentationBackend(), NeutralMaterialBackend(), ProxyIntrinsicBackend()
        wrap = lambda backend: MeasuredBackend(backend, rows, real)
        start = time.perf_counter()
        package = ReconstructionPipeline(geometry_backend=wrap(geometry), segmentation_backend=wrap(segmentation),
                    material_backend=wrap(material)).run(input_path, directory/'reconstruction', size, stage_event=event)
        package = export_intrinsic(package, directory/'intrinsic', wrap(intrinsic), event)
        package = export_saved(package, directory/'lighting', IntrinsicAssistedBackend(), event)
        light = json.loads((package/'lighting/lighting.json').read_text(encoding='utf-8'))
        # Explicit diagnostic scale, not a claim that an ambiguous fit is calibrated.
        package = export_shadows(package, directory/'final', shading_scale=light['fit']['normalization'], event=event)
        total = time.perf_counter()-start
        anchor = json.loads((package/'relighting/relighting.json').read_text(encoding='utf-8'))
        report['runs'].append(dict(index=index, analysisMaxSize=size, seconds=total, backends=rows, stages=events,
                                  source=anchor, finalPackage=str(package.relative_to(output))))
        write_json_atomic(output/'profile.json', report)
    return report


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--sizes', type=int, nargs='+', default=[512, 512, 256])
    p.add_argument('--real', action='store_true')
    a = p.parse_args()
    if any(not 128 <= size <= 1024 for size in a.sizes):
        p.error('sizes must be 128..1024; anchor retains native size')
    run(a.input, a.output, a.sizes, a.real)
