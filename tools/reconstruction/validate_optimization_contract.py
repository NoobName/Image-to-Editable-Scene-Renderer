"""Strict optimization boundary and deliberately incorrect finite CPU candidate for GPU rejection."""
import argparse,json,hashlib,shutil,subprocess
from pathlib import Path
from tempfile import TemporaryDirectory
import numpy as np
from runtime_dds import read_dds,write_dds

def run(validator,proposal,source,bad_output):
    validator=Path(validator).resolve();proposal=Path(proposal).resolve();source=Path(source).resolve();records=[]
    with TemporaryDirectory(prefix='optimization-contract-') as tmp:
        root=Path(tmp)/'优化副本';shutil.copytree(proposal,root);original=json.loads((root/'optimization.json').read_text(encoding='utf-8'))
        for case in ('valid','version','unknown','nan','loss','iterations','registered','hash','size','exposure','initial-state','energy'):
            j=json.loads(json.dumps(original));r=json.loads((proposal/'reference.json').read_text(encoding='utf-8'))
            if case=='version':j['version']=2
            if case=='unknown':j['extra']=True
            if case=='nan':j['bestLoss']=float('nan')
            if case=='loss':j['bestLoss']=j['initialLoss']+1
            if case=='iterations':j['maxIterations']=201
            if case=='registered':j['registered']=False
            if case=='hash':j['candidate']['sha256']='0'*64
            if case=='size':j['candidate']['size'][0]+=1
            if case=='exposure':j['exposureDeltaBounds']=[-2,2]
            if case=='initial-state':j['initialState']['displayExposure']=1
            if case=='energy':r['proposal']['target']['directIntensity']*=.5
            (root/'optimization.json').write_text(json.dumps(j),encoding='utf-8');(root/'reference.json').write_text(json.dumps(r),encoding='utf-8')
            result=subprocess.run([str(validator),str(root),str(source)],capture_output=True,text=True,encoding='utf-8',errors='replace')
            assert (result.returncode==0)==(case=='valid'),(case,result.stdout,result.stderr)
            records.append({'case':case,'exit':result.returncode,'diagnostic':result.stdout+result.stderr})
    bad_output=Path(bad_output);assert not bad_output.exists();shutil.copytree(proposal,bad_output)
    path=bad_output/'debug/candidate.dds';fmt,values=read_dds(path);values[...,:3]+=.1;write_dds(path,values,fmt)
    j=json.loads((bad_output/'optimization.json').read_text(encoding='utf-8'));j['candidate']['sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
    (bad_output/'optimization.json').write_text(json.dumps(j),encoding='utf-8')
    print(json.dumps(records,ensure_ascii=False,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('validator');p.add_argument('proposal');p.add_argument('source');p.add_argument('bad_output');a=p.parse_args();run(a.validator,a.proposal,a.source,a.bad_output)
