"""Stage24 deterministic ground truth and three saved real intrinsic comparisons; no model inference."""
from pathlib import Path
from dataclasses import replace
import json
import numpy as np
from PIL import Image
from tests.test_diffuse import fixture, KnownIntrinsic, metrics
from pipeline.geometry_backend import DummyGeometryBackend
from pipeline.material_estimation_backend import NeutralMaterialBackend
from pipeline.runner import ReconstructionPipeline
from estimate_intrinsic import export_intrinsic
from export_analysis import export_saved
from pipeline.diffuse_backend import IntrinsicAssistedBackend

def main():
    o,d,m,_=fixture(); root=Path('generated/prompt24-synthetic-final');root.mkdir(exist_ok=False)
    Image.fromarray(o.rgb).save(root/'input.png')
    class Geometry(DummyGeometryBackend):
        def predict(self,image):
            g=super().predict(image);return replace(g,normal=o.normal,metadata={**g.metadata,'backend':'synthetic-normal-field'})
    class Material(NeutralMaterialBackend):
        def predict(self,image):
            a=super().predict(image);return replace(a,albedo=o.albedo,roughness=o.roughness,metallic=o.metallic,confidence=o.material_confidence,
                metadata={**a.metadata,'backend':'synthetic-biased-material','albedo_source':'intrinsic'})
    ReconstructionPipeline(geometry_backend=Geometry(),material_backend=Material()).run(root/'input.png',root/'baseline',max_size=97)
    export_intrinsic(root/'baseline',root/'intrinsic',KnownIntrinsic())
    export_saved(root/'intrinsic',root/'assisted',IntrinsicAssistedBackend())
    report={'synthetic':metrics(o,d,m),'real':{}}
    for name in ('indoor','outdoor','painting'):
        output=Path('generated')/f'scene24-final-{name}'
        export_saved(Path('generated')/f'scene23-{name}',output,IntrinsicAssistedBackend())
        report['real'][name]=json.loads((output/'lighting/lighting.json').read_text(encoding='utf-8'))['assistance']
    export_saved(Path('generated/scene19-final'),Path('generated/prompt24-missing-final'),IntrinsicAssistedBackend())
    Path('generated/prompt24-comparison.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
