"""Create a two-depth fixture and measure translation parallax in real DX12 captures."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image
from pipeline.geometry_backend import GeometryPrediction, pinhole_points
from pipeline.geometry_builder import GeometryBuilder
from pipeline.scene_exporter import SceneExporter
from pipeline.types import InputImage, AnalysisResult, MaterialPrediction
from scene_package import load_package, write_package


def prepare(root):
    if root.exists():
        raise FileExistsError(f"Use a new fixture directory: {root}")
    width,height = 160,120
    rgb = np.empty((height,width,3),np.uint8)
    rgb[:,:width//2],rgb[:,width//2:] = [240,40,40],[40,80,240]
    depth = np.full((height,width),2,np.float32)
    depth[:,width//2:] = 4
    normal = np.zeros((height,width,3),np.float32)
    normal[:,:,2] = -1
    k = np.array([[height/width,0,.5],[0,1,.5],[0,0,1]],np.float32)
    image = InputImage(Path("two-depth-parallax.png"),rgb,(width,height),hashlib.sha256(rgb.tobytes()).hexdigest())
    prediction = GeometryPrediction(depth,normal,pinhole_points(depth,k),k,np.ones_like(depth),
                                    np.ones_like(depth,dtype=bool),"synthetic",{"dummy":True,"fixture":"2m-red-4m-blue"})
    analysis = AnalysisResult(depth,normal,np.ones_like(depth,dtype=np.uint32),MaterialPrediction(rgb),prediction)
    mesh = GeometryBuilder().build(image,analysis)
    for name,offset in (("base",0),("translated",.1)):
        package = SceneExporter().export(root/name,image,analysis,mesh,{"geometry":"analytic-parallax-fixture"})
        manifest = load_package(package)
        manifest["camera"]["position"][0] += offset
        manifest["camera"]["target"][0] += offset  # Translation only: no change in orientation.
        write_package(package,manifest)
    print(f"Prepared {root}; render base and translated in Albedo without UI, at equal size")


def compare(first,second):
    a,b = (np.array(Image.open(p).convert("RGB"),dtype=np.float32) for p in (first,second))
    if a.shape != b.shape:
        raise ValueError("Capture dimensions differ")
    measured = {}
    for name,channel,other,z in (("near",0,2,2),("far",2,0,4)):
        centers = []
        for image in (a,b):
            mask = (image[:,:,channel]>180)&(image[:,:,other]<100)&(image[:,:,1]<110)
            if mask.sum()<100:
                raise ValueError(f"Missing {name} color patch; use Albedo, no UI and the generated fixture")
            centers.append(float(np.nonzero(mask)[1].mean()))
        shift = centers[1]-centers[0]
        expected = -a.shape[0]*.1/z  # fy(normalized)=1; actual framebuffer height determines focal pixels.
        if abs(shift-expected)>1:
            raise ValueError(f"{name} parallax: measured {shift}, expected {expected} pixels")
        measured[name] = {"measured_pixels":shift,"expected_pixels":expected,"depth_meters":z}
    ratio = measured["near"]["measured_pixels"]/measured["far"]["measured_pixels"]
    if abs(ratio-2)>.08:
        raise ValueError(f"Expected near/far disparity ratio 2; got {ratio}")
    print(json.dumps({"translation_meters":.1,"size":[a.shape[1],a.shape[0]],"parallax":measured,
                      "near_far_ratio":ratio,"passed":True},indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command",required=True)
    sub.add_parser("prepare").add_argument("output",type=Path)
    check = sub.add_parser("compare")
    check.add_argument("base",type=Path)
    check.add_argument("translated",type=Path)
    args = parser.parse_args()
    if args.command == "prepare":
        prepare(args.output)
    else:
        compare(args.base,args.translated)


if __name__ == "__main__":
    main()
