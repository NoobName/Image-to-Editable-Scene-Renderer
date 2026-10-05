"""Verify generated Prompt 16 examples and finite GPU runs; retain machine-readable evidence."""
import argparse
import json
import locale
from pathlib import Path
import subprocess
import sys
import numpy as np
from PIL import Image
from appearance_contract import load_extension
from pipeline.appearance_debug import write_diagnostic


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root",type=Path)
    parser.add_argument("--validator",type=Path,action="append",required=True)
    args=parser.parse_args()
    project=Path(__file__).resolve().parents[2]
    root=args.root.resolve()
    report={"python":sys.version,"utf8Mode":sys.flags.utf8_mode,"localeEncoding":locale.getencoding(),"packages":{}}
    names=("analysis512","搬移目录/analysis256","exif6","legacy","legacy-upgraded","bad-size","bad-hash")
    for name in names:
        package=root/name
        expected=not name.startswith("bad-")
        entry={"expectedValid":expected}
        command=[sys.executable,str(Path(__file__).with_name("inspect_anchor.py")),str(package)]
        run=subprocess.run(command,capture_output=True,encoding="utf-8",env={**__import__("os").environ,"PYTHONIOENCODING":"utf-8"})
        entry["python"]={"exit":run.returncode,"diagnostic":run.stderr.strip()}
        assert (run.returncode==0)==expected,(name,run.stderr)
        entry["cpp"]=[]
        for validator in args.validator:
            run=subprocess.run([str(validator.resolve()),str(package),"--appearance"],capture_output=True,encoding="utf-8")
            entry["cpp"].append({"executable":str(validator),"exit":run.returncode,"diagnostic":run.stderr.strip()})
            assert (run.returncode==0)==expected,(name,validator,run.stderr)
        if expected:
            metadata=load_extension(package)
            entry["data"]=write_diagnostic(package,metadata) if metadata else {"status":"absent","3dCompatible":True}
        report["packages"][name]=entry
    first=report["packages"]["analysis512"]["data"]
    moved=report["packages"]["搬移目录/analysis256"]["data"]
    assert first["sourceSha256"]==moved["sourceSha256"] and first["sourceId"]==moved["sourceId"]
    assert first["sourceSize"]==[1500,1000] and first["analysisSize"]==[512,341]
    assert report["packages"]["exif6"]["data"]["sourceSize"]==[1000,1500]
    before=np.array(Image.open(project/"generated/prompt16-before.bmp").convert("RGB"))
    after=np.array(Image.open(project/"generated/prompt16-after.bmp").convert("RGB"))
    changed=int(np.count_nonzero(np.any(before!=after,axis=2)))
    report["oldFinalChangedPixels"]=changed
    assert changed==0,changed
    report["gpu"]={}
    for name in ("after","new-ui","exif","warp","release","reload"):
        log=(project/f"generated/prompt16-{name}.log").read_text(encoding="utf-8")
        assert "nonfinite=0" in log and "Completed frames=" in log,name
        if name!="release":assert "Validation summary: errors=0 warnings=0" in log,name
        lines=[line for line in log.splitlines() if any(key in line for key in ("HDR statistics:","Validation summary:","Completed frames=","Reconstruction Ready:","Resize:","Smoke:"))]
        report["gpu"][name]=lines
    assert sum("Reconstruction Ready:" in line for line in report["gpu"]["reload"])==2
    target=project/"generated/prompt16-results.json"
    target.write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)+"\n",encoding="utf-8")
    print(f"PASS: 7 package cases in Python + {len(args.validator)} C++ validators; old Final changed pixels={changed}; 6 GPU runs. {target}")


if __name__=="__main__":
    main()
