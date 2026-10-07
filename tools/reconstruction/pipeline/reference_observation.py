"""Use the same CPU observation contracts for saved packages and optional cached adapters."""
from pathlib import Path
from io import BytesIO
import hashlib
from PIL import Image
import numpy as np
from .image_io import load_image
from .types import AnalysisResult
from .geometry_backend import DummyGeometryBackend,validate_prediction
from .material_estimation_backend import NeutralMaterialBackend,validate_material
from .lighting_backend import make_lighting_input
from .saved_prediction import load_saved_geometry
from .saved_segmentation import load_saved_segmentation
from .material_assets import load_saved_material
from appearance_contract import load_extension,sha256_file
from scene_package import load_package


def reference_observation(path,max_size=512,geometry_backend='dummy',material_backend='neutral',allow_fallback=False):
    path=Path(path).resolve(strict=True)
    if path.name=='scene.json':path=path.parent
    reasons=[]
    if path.is_dir():
        load_package(path)
        image,g,report=load_saved_geometry(path)
        segmentation=load_saved_segmentation(path,image,g)
        material=load_saved_material(path,image)
        labels=segmentation.labels if segmentation is not None else np.ones_like(g.depth,dtype=np.uint32)
        analysis=AnalysisResult(g.depth,g.normal,labels,material,g,segmentation)
        anchor=load_extension(path)
        provenance={'inputKind':'saved-package','geometry':report['geometry_estimation'],
                    'material':getattr(material,'metadata',{}),'fallbackReasons':[]}
        return image,make_lighting_input(image,analysis),anchor,provenance
    image=load_image(path,max_size)
    geometry=DummyGeometryBackend();material=NeutralMaterialBackend()
    if geometry_backend=='moge':
        from .adapters.moge import MoGeGeometryBackend
        geometry=MoGeGeometryBackend(device='auto',offline=True,resolution_level=0)
    if material_backend=='marigold':
        from .adapters.marigold import MarigoldMaterialBackend
        material=MarigoldMaterialBackend('auto',True,512,4,3,13)
    try:
        try:g=geometry.predict(image)
        finally:geometry.release()
    except Exception as error:
        if not allow_fallback:raise
        reasons.append('geometry-adapter-unavailable: '+str(error));g=DummyGeometryBackend().predict(image)
    validate_prediction(image,g)
    try:
        try:m=material.predict(image)
        finally:
            if hasattr(material,'release'):material.release()
    except Exception as error:
        if not allow_fallback:raise
        reasons.append('material-adapter-unavailable: '+str(error));m=NeutralMaterialBackend().predict(image)
    validate_material(image,m)
    if g.metadata.get('backend','').startswith('dummy'):reasons.append('synthetic-normal-fallback; no observed direction')
    if m.metadata.get('albedo_source')!='intrinsic':reasons.append('neutral-albedo-fallback; object color is not illumination')
    analysis=AnalysisResult(g.depth,g.normal,np.ones_like(g.depth,dtype=np.uint32),m,g,None)
    canonical=BytesIO();Image.fromarray(image.canonical_rgb).save(canonical,format='PNG')
    anchor={'sourceId':'original-sha256:'+sha256_file(path),'sourceImage':{'sha256':hashlib.sha256(canonical.getvalue()).hexdigest(),'size':list(image.canonical_rgb.shape[1::-1])},
        'sourceCamera':{'status':'reconstructed-or-fallback','normalSpace':'lh-camera','normalizedIntrinsics':g.camera_intrinsics.tolist()}}
    return image,make_lighting_input(image,analysis),anchor,{'inputKind':'raw-image','geometry':g.metadata,'material':m.metadata,'fallbackReasons':reasons}
