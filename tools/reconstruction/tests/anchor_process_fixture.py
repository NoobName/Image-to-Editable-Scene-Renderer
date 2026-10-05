"""C++ integration fixture: successful process output containing a damaged optional sidecar."""
import argparse
import json
from pathlib import Path
import shutil

parser=argparse.ArgumentParser()
parser.add_argument("input",type=Path)
parser.add_argument("--output",type=Path,required=True)
parser.add_argument("--progress-file",type=Path,required=True)
parser.add_argument("--job-id",required=True)
args,_=parser.parse_known_args()
shutil.copytree(args.input.parent,args.output)
(args.output/"relighting").mkdir()
(args.output/"relighting/relighting.json").write_text('{"version":999}',encoding="utf-8")
args.progress_file.write_text(json.dumps({"version":1,"job_id":args.job_id,"sequence":10,"state":"complete",
    "stages":{name:"complete" for name in ("geometry","segmentation","materials","export")},
    "detail":"Test fixture: Python complete; sidecar intentionally damaged"}),encoding="utf-8")
