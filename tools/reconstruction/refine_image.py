"""Explicit offline physics-guided neural candidate. Core stays the usable fallback."""
import argparse
import copy
import hashlib
import json
import os
import shutil
import sys
from dataclasses import asdict
from pathlib import Path
from tempfile import TemporaryDirectory
import numpy as np
from PIL import Image
from package_schema import read_json, validate, _check_profile
from scene_package import write_json_atomic
from refinement_io import load_inputs, digest, accept_candidate, comparison_html
from pipeline.neural_refinement_backend import RefinementParameters, run_guarded, bypass_reason
from pipeline.adapters.pixl_refinement import PixlRefinementBackend, encode, ROOT, MODEL_REVISION, CODE_REVISION, ADAPTER_REVISION, WEIGHT_SHA256
SCHEMA=read_json(ROOT.parents[1]/'schemas/neural-refinement.schema.json')
_check_profile(SCHEMA)


def execute(recipe_path, physics_path, output, parameters, mask=None, cancel_file=None, use_cache=True, expected=None):
    output=Path(output).resolve(); recipe_path=Path(recipe_path).resolve(strict=True); physics_path=Path(physics_path).resolve(strict=True)
    if output.exists():raise FileExistsError('Choose a new refinement output directory; existing candidates are never overwritten')
    observation,recipe,inputs,package=load_inputs(recipe_path,physics_path,mask)
    if output.is_relative_to(package) or output.is_relative_to(physics_path):raise ValueError('Publish refinement outside immutable source/physics directories')
    parameters.validate()
    key_data={'inputs':inputs,'parameters':asdict(parameters),'modelRevision':MODEL_REVISION,'codeRevision':CODE_REVISION,'adapterRevision':ADAPTER_REVISION}
    key=hashlib.sha256(json.dumps(key_data,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    if expected and (expected['cacheKey']!=key or expected['inputs']!=inputs):raise ValueError('Replay input/parameter/revision hashes changed')
    def cancelled():return bool(cancel_file and Path(cancel_file).exists())
    if cancelled():raise RuntimeError('Cancelled before inference; physics retained')
    output.parent.mkdir(parents=True,exist_ok=True)
    cache=ROOT/'.cache/refinement'/key
    needs_model=bypass_reason(observation,parameters) is None
    backend=PixlRefinementBackend(); cache_hit=False
    if needs_model and use_cache and (cache/'cache.json').exists():
        record=read_json(cache/'cache.json')
        if record['key']!=key or digest(cache/'candidate.npz')!=record['sha256']:raise ValueError('Refinement cache corrupted; physics remains available')
        with np.load(cache/'candidate.npz',allow_pickle=False) as data:
            raw=data['rgb'].copy(); guidance=data['guidance'].copy()
        if raw.shape!=observation.physics_rgb.shape or raw.dtype!=np.float32 or not np.isfinite(raw).all():raise ValueError('Cached candidate invalid')
        if np.any(raw<0) or np.any(raw>1) or guidance.shape!=(1,9,parameters.max_side,parameters.max_side) or not np.isfinite(guidance).all():raise ValueError('Cached RGB/guidance contract invalid')
        provenance=record['provenance']; status='candidate'; cache_hit=True
    else:
        result=run_guarded(observation,parameters,backend if needs_model else None,cancelled)
        provenance=result.provenance; status=provenance['status']
        raw=backend.raw_candidate if status=='candidate' else observation.physics_rgb.copy()
        guidance=backend.debug_guidance
        if status=='candidate' and use_cache:
            cache.parent.mkdir(parents=True,exist_ok=True)
            if not cache.exists():
                with TemporaryDirectory(prefix='refinement-cache-',dir=cache.parent) as temporary:
                    staging=Path(temporary)/'entry'; staging.mkdir()
                    np.savez_compressed(staging/'candidate.npz',rgb=raw,guidance=guidance)
                    write_json_atomic(staging/'cache.json',{'key':key,'sha256':digest(staging/'candidate.npz'),'provenance':provenance})
                    if not cancelled():staging.rename(cache)
    if cancelled():raise RuntimeError('Cancelled after inference; candidate not published; physics retained')
    if status=='candidate':
        refined,weight,rejected,edges,metrics=accept_candidate(observation,raw,parameters.strength)
    else:
        refined=observation.physics_rgb.copy(); weight=np.zeros(refined.shape[:2],np.float32)
        rejected=np.ones(weight.shape,bool); edges=np.zeros_like(rejected)
        metrics={'protectedMaximumError':0.,'acceptedFraction':0.,'reason':provenance.get('reason'),'neuralInference':False}
    report={**key_data,'version':1,'status':status,'cacheKey':key,'cacheHit':cache_hit,'provenance':provenance,'metrics':metrics,
        'physicsRecipe':'physics-recipe.json','physicsExport':os.path.relpath(physics_path,output).replace('\\','/'),
        'protectedMask':None,'encoding':'sRGB RGB8 comparison/export; linear display-referred refinement, not HDR radiance',
        'modelChangesAreNotGeometryMeasurements':True,'defaultEnabled':False}
    with TemporaryDirectory(prefix='.'+output.name+'-refinement-',dir=output.parent) as temporary:
        staging=Path(temporary)/'result'; staging.mkdir()
        frozen=copy.deepcopy(recipe)
        frozen['sourcePackage']=os.path.relpath(package,output).replace('\\','/')
        if recipe['importedMask']:
            path=(recipe_path.parent/recipe['importedMask']['path']).resolve(strict=True)
            shutil.copyfile(path,staging/'imported-mask.png'); frozen['importedMask']['path']='imported-mask.png'
        write_json_atomic(staging/'physics-recipe.json',frozen)
        if mask:
            shutil.copyfile(mask,staging/'explicit-protection.png'); report['protectedMask']='explicit-protection.png'
        shutil.copyfile(physics_path/'original.png',staging/'original.png')
        shutil.copyfile(physics_path/'result.png',staging/'physics.png')
        Image.fromarray(encode(raw)).save(staging/'raw-candidate.png')
        if status!='candidate':shutil.copyfile(physics_path/'result.png',staging/'refined.png')
        else:Image.fromarray(encode(refined)).save(staging/'refined.png')
        for name,data in [('difference',np.clip(np.abs(refined-observation.physics_rgb)*4,0,1)),
                          ('protected',np.maximum(observation.protection,edges)),('accepted',weight),('rejected',rejected)]:
            Image.fromarray(np.round(np.asarray(data,np.float32)*255).astype(np.uint8)).save(staging/(name+'.png'))
        if guidance is not None:
            g=guidance[0]; view=np.concatenate([np.moveaxis(g[i:i+3],0,-1) for i in (0,3,6)],axis=1)
            Image.fromarray(encode(view)).save(staging/'guidance.png')
            np.savez_compressed(staging/'guidance.npz',target_intrinsics=guidance)
        else:
            from PIL import ImageDraw
            view=Image.new('RGB',(768,128),(25,30,36)); ImageDraw.Draw(view).text((16,50),'UNAVAILABLE: no neural inference performed',fill='white');view.save(staging/'guidance.png')
        report['outputHashes']={name:digest(staging/name) for name in ('original.png','physics.png','raw-candidate.png','refined.png','physics-recipe.json')}
        # Recipe hashing is semantic for relocatable paths; replay uses original frozen inputs
        # and verifies the export state again, never accepting stale UI state.
        report['originalRecipeHash']=inputs['recipe']
        validate(report,SCHEMA,schema=SCHEMA)
        comparison_html(staging/'comparison.html',status,metrics)
        write_json_atomic(staging/'refinement.json',report)
        if cancelled():raise RuntimeError('Cancelled before publication')
        staging.rename(output)
    print(json.dumps({'status':status,'output':str(output),'cacheHit':cache_hit,'metrics':metrics}),flush=True)
    return 0 if status in ('candidate','bypassed') else 2


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--recipe',type=Path);p.add_argument('--physics-export',type=Path);p.add_argument('--replay',type=Path)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--strength',type=float,default=0.)
    p.add_argument('--seed',type=int,default=34);p.add_argument('--max-side',type=int,choices=(256,512),default=256)
    p.add_argument('--protect-mask',type=Path);p.add_argument('--cancel-file',type=Path);p.add_argument('--no-cache',action='store_true')
    a=p.parse_args(); expected=None
    if a.replay:
        expected=read_json(a.replay)
        validate(expected,SCHEMA,schema=SCHEMA)
        if expected['version']!=1 or expected['adapterRevision']!=ADAPTER_REVISION or expected['modelRevision']!=MODEL_REVISION or expected['codeRevision']!=CODE_REVISION:raise ValueError('Unsupported refinement recipe revision')
        parent=a.replay.resolve().parent
        if Path(expected['physicsExport']).is_absolute() or '\\' in expected['physicsExport'] or ':' in expected['physicsExport']:raise ValueError('Physics export must use a relocatable relative path')
        a.recipe=parent/expected['physicsRecipe'];a.physics_export=parent/expected['physicsExport']
        a.protect_mask=parent/expected['protectedMask'] if expected['protectedMask'] else None
        if digest(a.recipe)!=expected['outputHashes']['physics-recipe.json']:raise ValueError('Frozen physics recipe changed')
        params=RefinementParameters(**expected['parameters'])
    else:params=RefinementParameters(a.strength,a.seed,a.max_side)
    if not a.recipe or not a.physics_export:p.error('Provide --recipe and --physics-export, or --replay')
    return execute(a.recipe,a.physics_export,a.output,params,a.protect_mask,a.cancel_file,not a.no_cache,expected)


if __name__=='__main__':
    try:raise SystemExit(main())
    except Exception as error:
        print(f'Refinement rejected; physics/source retained: {type(error).__name__}: {error}',file=sys.stderr)
        raise SystemExit(2)
