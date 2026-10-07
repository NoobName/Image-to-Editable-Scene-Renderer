"""Run the single pinned candidate on declared public comparison inputs; no geometry models."""
from pathlib import Path
import json
from pipeline.runner import ReconstructionPipeline
from pipeline.adapters.marigold_intrinsic import MarigoldIntrinsicBackend
from estimate_intrinsic import export_intrinsic

def main():
    rows=[]
    for name in ('outdoor','painting'):
        base=Path(f'generated/prompt23-{name}-base');output=Path(f'generated/scene23-{name}')
        ReconstructionPipeline().run(Path(f'generated/prompt23-inputs/{name}.jpg'),base,max_size=512)
        export_intrinsic(base,output,MarigoldIntrinsicBackend(offline=True,resolution=512,steps=4,ensemble=3,seed=23))
    for name in ('indoor','outdoor','painting'):
        root=Path(f'generated/scene23-{name}');data=json.loads((root/'intrinsic/intrinsic.json').read_text(encoding='utf-8'))
        rows.append(dict(input=name,package=str(root),analysisSize=data['analysisSize'],metrics=data['metrics'],provenance=data['provenance']))
    Path('generated/prompt23-comparison.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print(json.dumps(rows,indent=2))

if __name__=='__main__':main()
