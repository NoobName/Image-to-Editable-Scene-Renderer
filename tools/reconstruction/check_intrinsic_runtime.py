"""One final-adapter CUDA run with post-release memory accounting (no training)."""
import json
from pathlib import Path
import torch
import argparse
import gc
from estimate_intrinsic import export_intrinsic
from pipeline.adapters.marigold_intrinsic import MarigoldIntrinsicBackend

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=Path('generated/scene23-releasecheck-workspace'));args=p.parse_args()
    # cuBLAS owns a persistent per-process workspace. Establish that baseline before model ownership.
    scratch=torch.ones((32,32),device='cuda',dtype=torch.float16);product=scratch@scratch
    del scratch,product
    gc.collect();torch.cuda.synchronize();torch.cuda.empty_cache();baseline=torch.cuda.memory_allocated()
    export_intrinsic(Path('generated/scene19-final'),args.output,MarigoldIntrinsicBackend(offline=True))
    torch.cuda.synchronize()
    allocated=torch.cuda.memory_allocated();reserved=torch.cuda.memory_reserved()
    from pipeline.saved_prediction import load_saved_geometry
    image,_,_=load_saved_geometry(Path('generated/scene19-final'));backend=MarigoldIntrinsicBackend(offline=True,resolution=128,ensemble=1)
    backend.predict(image);backend.release();torch.cuda.synchronize()
    result=dict(cublasBaselineBytes=baseline,allocatedAfterRelease=allocated,reservedAfterRelease=reserved,allocatedAfterSecondRelease=torch.cuda.memory_allocated(),reservedAfterSecondRelease=torch.cuda.memory_reserved())
    Path('generated/prompt23-releasecheck.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(result)
    assert allocated==baseline==result['allocatedAfterSecondRelease'],result
