"""Exercise the C++ reference document boundary using temporary copies only."""
import argparse,json,shutil,subprocess
from pathlib import Path
from tempfile import TemporaryDirectory

def run(validator,proposal,source):
    validator=Path(validator).resolve();proposal=Path(proposal).resolve();source=Path(source).resolve();records=[]
    with TemporaryDirectory(prefix='reference-contract-') as tmp:
        root=Path(tmp)/'中文参考';shutil.copytree(proposal,root);original=json.loads((root/'reference.json').read_text(encoding='utf-8'))
        for case in ('valid','version','hash','missing','path','direction','range','identity','baseline','unsupported'):
            j=json.loads(json.dumps(original))
            if case=='version':j['version']=99
            if case=='hash':j['images']['reference']['sha256']='0'*64
            if case=='missing':j['images']['reference']['path']='textures/missing.png'
            if case=='path':j['images']['reference']['path']='../escape.png'
            if case=='direction':j['proposal']['target']['direction']=[0,0,0]
            if case=='range':j['proposal']['target']['directIntensity']=65
            if case=='identity':j['source']['sourceId']='wrong'
            if case=='baseline':j['source']['baselineRevision']=20
            if case=='unsupported':j['fit']['identifiable']=False
            (root/'reference.json').write_text(json.dumps(j),encoding='utf-8');result=subprocess.run([str(validator),str(root),str(source)],capture_output=True,text=True,encoding='utf-8',errors='replace')
            assert (result.returncode==0)==(case=='valid'),(case,result.stdout,result.stderr)
            records.append({'case':case,'exit':result.returncode,'diagnostic':result.stdout+result.stderr})
    print(json.dumps(records,ensure_ascii=False,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('validator');p.add_argument('proposal');p.add_argument('source');a=p.parse_args();run(a.validator,a.proposal,a.source)
