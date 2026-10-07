"""Offline bounded target optimization; produces a proposal, never edits a source package."""
import argparse,json,shutil,time
from pathlib import Path
from tempfile import TemporaryDirectory
import numpy as np
from PIL import Image,ImageDraw
from scene_package import load_package,write_json_atomic
from appearance_contract import load_extension,sha256_file
from runtime_dds import write_dds,read_dds
from pipeline.scene_exporter import check_destination
from pipeline.reference_observation import reference_observation
from pipeline.reference_backend import rgb_coefficients,LUMA
from pipeline.lighting_optimizer import optimize
from package_schema import validate


def diagnostic(path,result):
    canvas=Image.new('RGB',(960,580),'#111820');draw=ImageDraw.Draw(canvas)
    draw.text((16,12),'Fixed loss / best feasible parameters / masked diffuse residual',fill='white')
    values=[result.report['initialLoss']]+[x['bestLoss'] for x in result.report['history']];maximum=max(max(values),1e-10)
    points=[(20+i*440/max(len(values)-1,1),230-v/maximum*170) for i,v in enumerate(values)]
    if len(points)>1:draw.line(points,fill='#4ad2ff',width=3)
    draw.text((20,250),f"{values[0]:.7g} -> {values[-1]:.7g} | {result.report['status']}",fill='white')
    history=result.report['history']
    for axis in range(8):
        if len(history)<2:break
        seq=[h['parameters'][axis] for h in history];lo,hi=min(seq),max(seq)
        draw.line([(500+i*430/(len(seq)-1),220-(v-lo)/max(hi-lo,1e-9)*150) for i,v in enumerate(seq)],fill=['#ffcf52','#ffffff','#ff5050','#50ff70','#5080ff','#c77755','#77c799','#7777bb'][axis],width=2)
    draw.text((500,250),'yaw/pitch + RGB coefficients; each trace normalized',fill='white')
    for x,(name,data) in enumerate((('Mask',result.mask),('Residual x4',result.residual*4),('Initial shading /4',result.initial[...,:3]/4),('Best shading /4',result.best[...,:3]/4))):
        if data.ndim==2:data=np.repeat(data[...,None],3,axis=-1)
        panel=Image.fromarray(np.rint(np.clip(data,0,1)*255).astype(np.uint8)).resize((220,200))
        canvas.paste(panel,(20+235*x,320));draw.text((20+235*x,290),name,fill='white')
    draw.text((20,548),result.report['reason'][:125],fill='white');canvas.save(path)


def run(source,reference_analysis,output,request,registered=False,iterations=100,cancel_file=None):
    source=Path(source).resolve(strict=True);load_package(source);anchor=load_extension(source)
    ref=Path(reference_analysis).resolve(strict=True);ref=ref.parent if ref.is_file() else ref
    document=json.loads((ref/'reference.json').read_text(encoding='utf-8'));output=check_destination(Path(output))
    reference_schema=json.loads((Path(__file__).resolve().parents[2]/'schemas/reference-analysis.schema.json').read_text(encoding='utf-8'))
    validate(document,reference_schema,schema=reference_schema)
    for asset in document['images'].values():
        asset_path=(ref/asset['path']).resolve(strict=True)
        if not asset_path.is_relative_to(ref) or sha256_file(asset_path)!=asset['sha256']:raise ValueError('Reference preview identity/path mismatch')
    if source==output or source in output.parents or ref==output or ref in output.parents:raise ValueError('Optimization output must be a new directory outside observations')
    expected=request['source'];actual=document['source']
    same_identity=all(expected[k]==actual[k] for k in ('sourceId','anchorSha256','baselineRevision'))
    same_light=all(np.allclose(expected['baseline'][k],actual['baseline'][k],atol=1e-6,rtol=1e-6) for k in actual['baseline'])
    if not same_identity or not same_light or actual['sourceId']!=anchor['sourceId'] or actual['anchorSha256']!=anchor['sourceImage']['sha256']:raise ValueError('Optimization source baseline/identity mismatch')
    # C++ stores float32 lighting. Preserve its exact snapshot after tolerant cross-language verification.
    document['source']=actual=expected
    initial=request['initialState']['target'].copy();gain=request['initialState']['targetGlobalGain']
    initial['directIntensity']*=gain;initial['ambientIntensity']*=gain
    image,observation,_,_=reference_observation(source)
    # Reference proposal owns its fitted proxy/normal/mask, so optimization needs no model or original reference path.
    from dataclasses import replace
    def numeric(name):
        path=(ref/'debug'/name).resolve(strict=True)
        if not path.is_relative_to(ref):raise ValueError('Reference numeric asset escapes analysis directory')
        return read_dds(path)
    _,ref_normal=numeric('normal.dds');_,proxy=numeric('proxy.dds');_,fit_mask=numeric('fit-mask.dds')
    # Registered loss uses the fixed proxy directly below, not a re-estimated source albedo.
    reference=observation
    if document['proposal']['relation']=='same-scene':
        if ref_normal.shape[:2]!=observation.valid.shape:raise ValueError('Registered reference resolution mismatch')
        if document['reference']['sourceCamera']!=anchor['sourceCamera']:raise ValueError('Registered source/reference camera contract mismatch')
        _,ref_albedo=numeric('albedo.dds')
        with Image.open(ref/'textures/reference.png') as preview:ref_rgb=np.asarray(preview.convert('RGB'))
        if ref_rgb.shape!=observation.rgb.shape:raise ValueError('Registered reference needs unresized analysis RGB')
        reference=replace(observation,rgb=ref_rgb,albedo=ref_albedo[...,:3],normal=ref_normal[...,:3],valid=fit_mask.astype(bool))
    protection=np.zeros(observation.valid.shape,np.float32)
    for region in request['initialState']['protectedRegions']:protection[observation.labels==region['label']]=region['weight']
    if request.get('importedMask'):
        _,mask=read_dds(request['importedMask']);h,w=protection.shape;mh,mw=mask.shape
        protection=np.maximum(protection,mask[np.minimum(((np.arange(h)+.5)*mh/h).astype(int),mh-1)[:,None],np.minimum(((np.arange(w)+.5)*mw/w).astype(int),mw-1)[None,:]])
    started=time.perf_counter();output.parent.mkdir(parents=True,exist_ok=True)
    # Pass the saved proxy explicitly; reconstructing it through RGB8 would lose precision and repeat normalization.
    result=optimize(observation,reference,document,initial,actual['baseline'],document['proposal']['relation'],registered=registered,
        protection=protection,iterations=iterations,cancel=lambda:bool(cancel_file and Path(cancel_file).exists()),
        reference_proxy=proxy[...,:3],reference_mask=fit_mask,reference_normal=ref_normal[...,:3])
    with TemporaryDirectory(prefix='.'+output.name+'-optimization-',dir=output.parent) as temporary:
        root=Path(temporary)/'document';shutil.copytree(ref,root)
        # A new analysis document carries the target candidate; no source/reference file is overwritten.
        document['proposal']['target']=result.target if result.report['status']=='improved' else actual['baseline']
        document['proposal']['canApply']=result.report['status']=='improved';document['proposal']['directionSupported']=document['proposal']['canApply']
        if not document['proposal']['canApply']:document['proposal']['confidence']=0
        document['proposal']['reasons'].append('bounded-target-optimization: '+result.report['reason'])
        d,b=rgb_coefficients(document['proposal']['target']);stats=document['proposal']['statistics'];stats['targetEnergy']=float((d+b)@LUMA);stats['directAmbientRatio']=float(d@LUMA/max(b@LUMA,1e-6))
        stats['directionChangeDegrees']=float(np.degrees(np.arccos(np.clip(np.dot(actual['baseline']['direction'],document['proposal']['target']['direction']),-1,1))))
        record=result.report;record['seconds']=time.perf_counter()-started;record['initialState']=request['initialState'];record['initialEffectiveTarget']=initial
        for name,values,fmt in (('candidate',result.best,'RGBA32_FLOAT'),('initial',result.initial,'RGBA32_FLOAT'),('optimization-mask',result.mask,'R32_FLOAT'),('optimization-residual',result.residual,'R32_FLOAT')):
            write_dds(root/'debug'/f'{name}.dds',values.astype(np.float32),fmt)
        record['candidate']={'path':'debug/candidate.dds','sha256':sha256_file(root/'debug/candidate.dds'),'size':[image.width,image.height]}
        diagnostic(root/'debug/optimization.png',result)
        schema=json.loads((Path(__file__).resolve().parents[2]/'schemas/lighting-optimization.schema.json').read_text(encoding='utf-8'))
        validate(record,schema,schema=schema)
        write_json_atomic(root/'optimization.json',record);write_json_atomic(root/'reference.json',document)
        check_destination(output)
        if output.exists():output.rmdir()
        root.rename(output)
    return record


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-package',required=True,type=Path);p.add_argument('--reference-analysis',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path);p.add_argument('--request',required=True,type=Path);p.add_argument('--registered',action='store_true');p.add_argument('--iterations',type=int,default=100);p.add_argument('--cancel-file',type=Path);a=p.parse_args()
    try:
        r=run(a.source_package,a.reference_analysis,a.output,json.loads(a.request.read_text(encoding='utf-8')),a.registered,a.iterations,a.cancel_file)
        print(json.dumps({k:v for k,v in r.items() if k not in ('history','initialState')},ensure_ascii=False,indent=2))
    except Exception as error:print('Target optimization failed: '+str(error));raise SystemExit(1)
