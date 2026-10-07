"""Relocation and damaged-wrapper checks use independent copies, never live candidates."""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from refinement_io import digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    # Same depth/relative dependencies as the validated candidate. This is intentional:
    # arbitrary detached movement does not magically relocate the source package/export.
    if a.output.resolve().parent != a.candidate.resolve().parent.parent:
        raise ValueError('Use a sibling evidence root to preserve package/export path depth')
    cases=[]
    for name,change,code in [('中文候选',None,0),('bad-version','version',2),('bad-hash','hash',2),('missing-physics','missing',2)]:
        copied=a.output/name;shutil.copytree(a.candidate,copied)
        record=json.loads((copied/'refinement.json').read_text(encoding='utf-8'))
        if change=='version':record['version']=99
        if change=='hash':record['outputHashes']['physics-recipe.json']='0'*64
        if change=='missing':record['physicsExport']='../missing-export'
        (copied/'refinement.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
        target=a.output/(name+'-replayed')
        with (a.output/(name+'.log')).open('w',encoding='utf-8') as log:
            result=subprocess.run([sys.executable,str(Path(__file__).with_name('refine_image.py')),
                '--replay',str(copied/'refinement.json'),'--output',str(target)],stdout=log,stderr=subprocess.STDOUT,timeout=30)
        assert result.returncode==code,(name,result.returncode)
        if code:assert not target.exists()
        else:assert digest(target/'refined.png')==digest(a.candidate/'refined.png')
        cases.append({'name':name,'exit':result.returncode,'published':target.exists()})
    report={'cases':cases,'copiedOnly':True,'relativeLayoutPreserved':True,'unicodeReplayByteExact':True}
    (a.output/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
