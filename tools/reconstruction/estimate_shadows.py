"""Offline fixed-source old shadow support; writes a NEW package, never relights RGB."""
import argparse,json,shutil,time
from pathlib import Path
from tempfile import TemporaryDirectory
import numpy as np
from pipeline.saved_prediction import load_saved_geometry
from pipeline.saved_segmentation import load_saved_segmentation
from pipeline.scene_exporter import check_destination
from pipeline.shadow_backend import ShadowInput,DepthShellShadowBackend
from pipeline.shadow_assets import write_shadow,invalidate_shadow
from pipeline.progress import ProgressReporter,SHADOW_STAGES
from appearance_contract import load_extension
from intrinsic_contract import load_intrinsic,read_arrays
from runtime_dds import read_dds
from scene_package import load_package

def export_shadows(package,output,confirm=None,protect=None,shading_scale=None,steps=96,event=None):
    package=Path(package).resolve(strict=True);output=check_destination(Path(output));event=event or (lambda *args:None)
    if output.resolve().is_relative_to(package):raise ValueError('Shadow output must be outside input')
    image,g,_=load_saved_geometry(package);appearance=load_extension(package);iid=load_intrinsic(package,appearance)
    if iid is None or not (package/'lighting/lighting.json').is_file():raise ValueError('Old-shadow analysis requires saved intrinsic and source lighting')
    light=json.loads((package/'lighting/lighting.json').read_text(encoding='utf-8'));fit=light['fit']
    from shadow_contract import load_shadow
    previous=load_shadow(package,appearance)
    if previous:
        # A rerun preserves explicit human layers unless the caller supplies replacements.
        if confirm is None and 'confirm' in previous['manual']:confirm=package/previous['manual']['confirm']['path']
        if protect is None and 'protect' in previous['manual']:protect=package/previous['manual']['protect']['path']
    if shading_scale is None:
        if not light.get('assistance',{}).get('selected'):raise ValueError('Explicit --shading-scale required without accepted intrinsic-assisted source calibration')
        shading_scale=fit['normalization']
    meta=json.loads((package/'analysis/analysis.json').read_text(encoding='utf-8'))
    def load(k):
        if meta['maps'][k].get('path')!=f'analysis/{k}.dds':raise ValueError('Shadow backend requires standard exported runtime map paths')
        return read_dds(package/meta['maps'][k]['path'])[1]
    maps=read_arrays(package,iid);seg=load_saved_segmentation(package,image,g);excluded=np.zeros_like(g.valid_mask);names=[]
    if seg is not None:
        for region in seg.regions:
            if any(word in region.name.casefold() for word in ('sky','lamp','light','screen','television','emiss','glass','mirror')):excluded|=load('region')==region.label_id;names.append(region.name)
    confidence=1. if fit['backend']=='manual-test' else fit['confidence']
    if iid['provenance']['provenance']=='derived-not-estimated':confidence=0. # I/A proxy cannot prove a dark texture is a cast shadow.
    # Consume the fingerprinted runtime maps, not a second unfingerprinted NPZ copy.
    o=ShadowInput(load('depth'),load('normal')[...,:3],load('position')[...,:3],np.asarray(meta['camera']['intrinsicsNormalized']).reshape(3,3),
        (load('validity')!=0)&(maps['validity']!=0),load('region'),load('roughness'),load('metallic'),
        maps['albedo'][...,:3],maps['shading'][...,:3],maps['error'],maps['uncertainty'][...,1],excluded,light['sourceLighting'],float(shading_scale),confidence)
    event('shadow','running','Checking observed attenuation against fixed depth-shell occlusion support');started=time.perf_counter()
    results,parameters=DepthShellShadowBackend(steps).predict(o)
    diagnostics=dict(backend=DepthShellShadowBackend.name,candidatePixels=int((results['candidate']>0).sum()),unknownPixels=int(results['unknown'].sum()),lightConfidence=confidence,excludedNames=names,seconds=time.perf_counter()-started)
    event('shadow','complete',f"{diagnostics['candidatePixels']} candidates; unknown remains protected")
    output.parent.mkdir(parents=True,exist_ok=True)
    if any(p.is_symlink() for p in package.rglob('*')):raise ValueError('Shadow export refuses symbolic links')
    event('export','running','Publishing independent analysis; final renderer unchanged')
    with TemporaryDirectory(prefix=f'.{output.name}-shadow-',dir=output.parent) as staging:
        target=Path(staging)/'package';shutil.copytree(package,target);invalidate_shadow(target)
        write_shadow(target,appearance,results,parameters,diagnostics,confirm,protect);load_package(target);check_destination(output)
        if output.exists():output.rmdir()
        target.rename(output)
    event('export','complete','Shadow evidence published');return output

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('package',type=Path);p.add_argument('--output',required=True,type=Path)
    p.add_argument('--confirm-mask',type=Path);p.add_argument('--protect-mask',type=Path);p.add_argument('--shading-scale',type=float);p.add_argument('--steps',type=int,default=96)
    p.add_argument('--progress-file',type=Path);p.add_argument('--job-id',default='');a=p.parse_args()
    if a.progress_file and (a.progress_file.resolve()==a.output.resolve() or a.output.resolve() in a.progress_file.resolve().parents):p.error('Progress file must be outside output')
    reporter=ProgressReporter(a.progress_file,a.job_id,SHADOW_STAGES)
    try:print(export_shadows(a.package,a.output,a.confirm_mask,a.protect_mask,a.shading_scale,a.steps,reporter.stage));reporter.finish();return 0
    except Exception as e:reporter.fail(e);print(f'Shadow analysis failed: {e}');return 1
if __name__=='__main__':raise SystemExit(main())
