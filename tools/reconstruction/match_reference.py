"""Offline Reference -> inspectable target proposal. No RGB transfer and no target optimization."""
import argparse
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import time
import numpy as np
from PIL import Image
from appearance_contract import load_extension,sha256_file
from scene_package import load_package,write_json_atomic
from pipeline.scene_exporter import check_destination
from pipeline.reference_observation import reference_observation
from pipeline.reference_backend import ReferenceLightingMatcher,parameters,validate_light
from pipeline.lighting_assets import write_lighting_diagnostic
from runtime_dds import write_dds


def match(reference,source,output,relation='different-content',request=None,**options):
    source=Path(source).resolve(strict=True);load_package(source);anchor=load_extension(source)
    if anchor is None:raise ValueError('Source has no fixed appearance anchor')
    output=check_destination(Path(output))
    if output==source or source in output.parents:raise ValueError('Reference output must be outside source package')
    reference=Path(reference).resolve(strict=True)
    reference_root=reference if reference.is_dir() else reference.parent if reference.name=='scene.json' else None
    if reference_root and (output==reference_root or reference_root in output.parents):raise ValueError('Output must be outside reference package')
    if request is None:
        light=json.loads((source/'lighting/lighting.json').read_text(encoding='utf-8'))['sourceLighting']
        request={'sourceId':anchor['sourceId'],'anchorSha256':anchor['sourceImage']['sha256'],'baseline':parameters(light),'baselineRevision':0}
    if request['sourceId']!=anchor['sourceId'] or request['anchorSha256']!=anchor['sourceImage']['sha256']:
        raise ValueError('Source request identity mismatch')
    validate_light(request['baseline'])
    started=time.perf_counter();image,observation,ref_anchor,provenance=reference_observation(reference,**options)
    matcher=ReferenceLightingMatcher();analysis=matcher.analyze(observation,provenance);proposal=matcher.propose(analysis,request['baseline'],relation)
    output.parent.mkdir(parents=True,exist_ok=True)
    with TemporaryDirectory(prefix='.'+output.name+'-reference-',dir=output.parent) as temporary:
        root=Path(temporary)/'document';root.mkdir();(root/'textures').mkdir();(root/'debug').mkdir()
        # A labelled preview is permitted to be smaller than its canonical reference asset.
        # The source anchor itself and source/analysis mapping are never changed.
        preview=Image.fromarray(image.rgb);preview.thumbnail((1024,1024));preview.save(root/'textures/reference.png')
        write_lighting_diagnostic(root/'debug',analysis.estimate)
        residual=Image.open(root/'debug/residual.png').convert('RGB');residual.save(root/'textures/residual.png')
        def asset(path):
            with Image.open(root/path) as im:size=list(im.size)
            return {'path':path,'sha256':sha256_file(root/path),'size':size}
        for name,values in [('normal',observation.normal),('albedo',observation.albedo),('proxy',analysis.estimate.proxy),('fitted',analysis.estimate.old_shading)]:
            write_dds(root/'debug'/f'{name}.dds',np.concatenate((values,np.zeros((*values.shape[:2],1),np.float32)),-1).astype(np.float32),'RGBA32_FLOAT')
        write_dds(root/'debug/residual.dds',analysis.estimate.residual,'R32_FLOAT')
        write_dds(root/'debug/fit-mask.dds',analysis.estimate.fit_mask,'R32_UINT')
        data={'version':1,'source':request,'reference':{'sourceId':ref_anchor['sourceId'],'sourceSha256':ref_anchor['sourceImage']['sha256'],
            'sourceSize':ref_anchor['sourceImage']['size'],'analysisSize':[image.width,image.height],'sourceCamera':ref_anchor.get('sourceCamera',{}),
            'normalSpace':'lh-camera','provenance':provenance},'proposal':proposal,'fit':analysis.estimate.fit,
            'images':{'reference':asset('textures/reference.png'),'residual':asset('textures/residual.png')},
            'seconds':time.perf_counter()-started,'notes':['Same-scene is caller-declared, not automatic registration.',
                'Different-content uses fitted illumination statistics only, never pixel RGB alignment.',
                'Direction copies camera-relative XYZ; common world coordinates and absolute radiance are unavailable.']}
        write_json_atomic(root/'reference.json',data)
        check_destination(output)
        if output.exists():output.rmdir()
        root.rename(output)
    return data


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('reference',type=Path);parser.add_argument('--source-package',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path);parser.add_argument('--relation',choices=('same-scene','different-content'),default='different-content')
    parser.add_argument('--request',type=Path);parser.add_argument('--geometry-backend',choices=('dummy','moge'),default='dummy')
    parser.add_argument('--material-backend',choices=('neutral','marigold'),default='neutral');parser.add_argument('--max-size',type=int,default=512)
    parser.add_argument('--allow-fallback',action='store_true');a=parser.parse_args()
    try:
        request=json.loads(a.request.read_text(encoding='utf-8')) if a.request else None
        result=match(a.reference,a.source_package,a.output,a.relation,request,geometry_backend=a.geometry_backend,material_backend=a.material_backend,max_size=a.max_size,allow_fallback=a.allow_fallback)
        print(json.dumps({'proposal':result['proposal'],'fit':result['fit'],'seconds':result['seconds']},ensure_ascii=False,indent=2));return 0
    except Exception as error:print('Reference analysis failed: '+str(error));return 1


if __name__=='__main__':raise SystemExit(main())
