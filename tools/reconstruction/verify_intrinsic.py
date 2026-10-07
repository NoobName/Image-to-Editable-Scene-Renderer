"""Check real model evidence, byte-preserved inputs, numeric readback and unchanged baseline."""
import json
from pathlib import Path
import numpy as np
from PIL import Image
from intrinsic_contract import load_intrinsic,read_arrays
from appearance_contract import load_extension
from pipeline.adapters.intrinsic_config import MODEL_REVISION,WEIGHT_HASHES

def main():
    rows=[]
    for name,base in [('indoor','scene19-final'),('outdoor','prompt23-outdoor-base'),('painting','prompt23-painting-base')]:
        root=Path('generated')/f'scene23-{name}';old=Path('generated')/base;data=load_intrinsic(root,load_extension(root));arrays=read_arrays(root,data)
        for p in old.rglob('*'):
            if p.is_file():assert (root/p.relative_to(old)).read_bytes()==p.read_bytes(),p
        assert data['provenance']['model_revision']==MODEL_REVISION and data['provenance']['weight_sha256']==WEIGHT_HASHES
        error=arrays['error'];unc=arrays['uncertainty'][...,:3];residual=arrays['residual'][...,:3]
        rows.append(dict(name=name,metrics=data['metrics'],errorP50=float(np.percentile(error,50)),errorP95=float(np.percentile(error,95)),uncertaintyMeanASR=unc.mean(axis=(0,1)).tolist(),residualMeanRGB=residual.mean(axis=(0,1)).tolist(),sourceBytesPreserved=True,provenance=data['provenance']))
    count=0
    for directory in Path('generated').glob('prompt23-*-intrinsic'):
        data=json.loads((directory/'readback.json').read_text(encoding='utf-8'));count+=1
        for key,v in data.items():assert v['byteExact'] and v['nonfinite']==0 and v['stateBefore']==v['stateAfter']==128 and v['stateCapture']==2048
    for path in Path('generated').glob('prompt23-*.bmp'):
        if 'release' not in path.stem:assert 'Validation summary: errors=0 warnings=0' in path.with_suffix('.log').read_text(encoding='utf-8-sig')
    for key in ('old','new','ratio','result'):
        assert Path(f'generated/prompt23-ratio-drag-shading/{key}.bin').read_bytes()==Path(f'generated/prompt23-baseline-shading/{key}.bin').read_bytes(),key
    a=np.asarray(Image.open('generated/prompt23-relighted.bmp'),dtype=int);b=np.asarray(Image.open('generated/prompt23-original.bmp'),dtype=int);assert abs(a-b).max()<=1
    assert Path('generated/prompt23-final.bmp').read_bytes()==Path('generated/prompt22-final.bmp').read_bytes()
    result=dict(comparisons=rows,readbackSets=count,baselineBuffersByteExact=True,identityMaxLSB=int(abs(a-b).max()),finalByteExact=True)
    Path('generated/prompt23-verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2))

if __name__=='__main__':main()
