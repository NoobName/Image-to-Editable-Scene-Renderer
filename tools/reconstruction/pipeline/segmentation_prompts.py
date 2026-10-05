"""Named point/box prompts in normalized processed-image coordinates."""
import json
import math
from pathlib import Path
from .segmentation_types import CATEGORIES


def load_prompts(path):
    path = Path(path)
    if path.stat().st_size>1024*1024:
        raise ValueError("Segmentation prompt JSON exceeds 1 MiB")
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(data,dict) or set(data)!={"version","regions"} or type(data["version"]) is not int or data["version"]!=1:
        raise ValueError("Prompt file requires version=1 and regions")
    if not isinstance(data["regions"],list) or not 1<=len(data["regions"])<=127:
        raise ValueError("Prompt file needs 1..127 regions")
    ids = set()
    for p in data["regions"]:
        if not isinstance(p,dict) or set(p)-{"id","name","category","points","box"} or not {"id","name","category"}<=set(p):
            raise ValueError("Each prompt requires id/name/category and points or box")
        import re
        if not isinstance(p["id"],str) or not re.fullmatch(r"[a-zA-Z0-9_-]{1,64}",p["id"]) or p["id"] in ids:
            raise ValueError("Prompt IDs must be unique letters/digits/_/-")
        ids.add(p["id"])
        if not isinstance(p["name"],str) or not p["name"].strip() or len(p["name"])>128 or p["category"] not in CATEGORIES:
            raise ValueError("Invalid prompt name/category")
        def coordinate(x):
            return type(x) in (int,float) and math.isfinite(x) and 0<=x<=1
        points = p.get("points",[])
        if not isinstance(points,list) or len(points)>128:
            raise ValueError("Prompt points must be a list with at most 128 entries")
        for point in points:
            if not isinstance(point,list) or len(point)!=3 or not all(coordinate(v) for v in point[:2]) or type(point[2]) is not int or point[2] not in (0,1):
                raise ValueError("Points must be [u,v,label], u/v in [0,1], integer label 0 or 1")
        box = p.get("box")
        if box is not None and (not isinstance(box,list) or len(box)!=4 or not all(coordinate(v) for v in box) or box[0]>=box[2] or box[1]>=box[3]):
            raise ValueError("Box must be [u0,v0,u1,v1] in [0,1] with positive extent")
        if box is None and not any(point[2]==1 for point in points):
            raise ValueError("Prompt requires a box or a positive point")
    return tuple(data["regions"])
