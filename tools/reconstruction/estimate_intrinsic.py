"""Offline intrinsic-only update: independent process, immutable inputs, atomic NEW package."""
import argparse
import shutil
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from pipeline.saved_prediction import load_saved_geometry
from pipeline.material_assets import load_saved_material
from pipeline.scene_exporter import check_destination
from pipeline.intrinsic_backend import ProxyIntrinsicBackend,SavedIntrinsicBackend
from pipeline.intrinsic_assets import write_intrinsic
from pipeline.progress import ProgressReporter,INTRINSIC_STAGES
from appearance_contract import load_extension
from scene_package import load_package

def export_intrinsic(package,output,backend,event=None):
    package=Path(package).resolve(strict=True);output=check_destination(Path(output));event=event or (lambda *args:None)
    if output.resolve().is_relative_to(package):raise ValueError('New intrinsic package must be outside the input package')
    image,_,_=load_saved_geometry(package);material=load_saved_material(package,image);appearance=load_extension(package)
    if appearance is None:raise ValueError('Intrinsic update requires an existing source anchor; export_analysis first')
    if any(p.is_symlink() for p in package.rglob('*')):raise ValueError('Intrinsic update refuses symbolic links')
    event('intrinsic','running',backend.name)
    try:result=backend.predict(image,material)
    finally:backend.release()
    event('intrinsic','complete',result.metadata['provenance']);event('export','running','Publishing a new package; original/material/lighting bytes retained')
    output.parent.mkdir(parents=True,exist_ok=True)
    with TemporaryDirectory(prefix=f'.{output.name}-intrinsic-',dir=output.parent) as staging:
        target=Path(staging)/'package';shutil.copytree(package,target)
        # Replace only this derived sidecar in the private staging copy. The old package remains immutable.
        old_intrinsic=(target/'intrinsic').resolve()
        if not old_intrinsic.is_relative_to(Path(staging).resolve()):raise ValueError('Staging intrinsic path escaped its private workspace')
        if old_intrinsic.exists():shutil.rmtree(old_intrinsic)
        write_intrinsic(target,image,appearance,result)
        from pipeline.shadow_assets import invalidate_shadow
        invalidate_shadow(target)
        lighting=target/'lighting/lighting.json'
        if lighting.exists() and 'assistance' in json.loads(lighting.read_text(encoding='utf-8')):
            # A new decomposition invalidates fits fingerprinting the previous intrinsic observation.
            # Delete only the private copied derived directory; source lighting remains in the old package.
            derived=lighting.parent.resolve()
            if not derived.is_relative_to(Path(staging).resolve()):raise ValueError('Derived lighting escaped staging')
            shutil.rmtree(derived)
        load_package(target);check_destination(output)
        if output.exists():output.rmdir()
        target.rename(output)
    event('export','complete','ScenePackage published');return output

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('package',type=Path);p.add_argument('--output',required=True,type=Path)
    p.add_argument('--backend',choices=('marigold-lighting','proxy','saved'),default='marigold-lighting');p.add_argument('--offline',action='store_true')
    p.add_argument('--device',choices=('auto','cuda','cpu'),default='auto');p.add_argument('--resolution',type=int,default=512);p.add_argument('--steps',type=int,default=4);p.add_argument('--ensemble',type=int,default=3);p.add_argument('--seed',type=int,default=23)
    p.add_argument('--progress-file',type=Path);p.add_argument('--job-id',default='');args=p.parse_args()
    if args.progress_file and (args.progress_file.resolve()==args.output.resolve() or args.output.resolve() in args.progress_file.resolve().parents):p.error('Progress file must be outside output')
    reporter=ProgressReporter(args.progress_file,args.job_id,INTRINSIC_STAGES)
    try:
        if args.backend=='proxy':backend=ProxyIntrinsicBackend()
        elif args.backend=='saved':backend=SavedIntrinsicBackend(args.package)
        else:
            from pipeline.adapters.marigold_intrinsic import MarigoldIntrinsicBackend
            backend=MarigoldIntrinsicBackend(args.device,args.offline,args.resolution,args.steps,args.ensemble,args.seed)
        print(export_intrinsic(args.package,args.output,backend,reporter.stage));reporter.finish();return 0
    except Exception as error:reporter.fail(error);print(f'Intrinsic estimation failed: {error}');return 1

if __name__=='__main__':raise SystemExit(main())
