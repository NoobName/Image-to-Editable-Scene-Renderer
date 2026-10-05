"""Create owned, reproducible Prompt 16 diagnostic fixtures. Never overwrite a previous run."""
import argparse
import json
from pathlib import Path
import shutil
from PIL import Image, ImageDraw, ImageCms
from pipeline.runner import ReconstructionPipeline
from pipeline.geometry_builder import GeometryBuilder
from pipeline.saved_prediction import remesh_saved
from pipeline.scene_exporter import check_destination
from appearance_contract import load_extension, SIDECAR


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output",type=Path)
    args=parser.parse_args()
    root=check_destination(args.output)
    root.mkdir(parents=True,exist_ok=True)
    (root/"inputs").mkdir()
    image=Image.new("RGB",(1500,1000),"white")
    draw=ImageDraw.Draw(image)
    for y in range(15,1000,24):
        draw.text((14,y),f"Pixel row {y:04d} | Appearance anchor: fine text ABC 0123456789 "*4,fill=(25,35,55))
    draw.rectangle((1000,100,1460,500),fill=(40,170,95))
    draw.ellipse((1100,580,1380,860),fill=(40,95,220))
    profile=ImageCms.ImageCmsProfile(ImageCms.createProfile("sRGB")).tobytes()
    source=root/"inputs/细字 ICC.png";image.save(source,icc_profile=profile)
    exif=Image.Exif();exif[274]=6
    oriented=root/"inputs/旋转 EXIF6.jpg";image.save(oriented,quality=95,exif=exif,icc_profile=profile)
    pipeline=ReconstructionPipeline(geometry_builder=GeometryBuilder(grid_size=33))
    for name,path,maximum in (("analysis512",source,512),("analysis256",source,256),("exif6",oriented,512)):
        pipeline.run(path,root/name,max_size=maximum)
    # Model older packages by copying only their existing v1 assets. Original package remains untouched.
    legacy=root/"legacy"
    shutil.copytree(root/"analysis512",legacy,ignore=shutil.ignore_patterns("relighting","analysis","source_anchor.png"))
    remesh_saved(legacy,root/"legacy-upgraded",GeometryBuilder(grid_size=33))
    for label in ("bad-size","bad-hash"):
        target=root/label;shutil.copytree(root/"analysis512",target)
        data=load_extension(target)
        if label=="bad-size":data["sourceImage"]["size"][0]+=1
        else:data["sourceImage"]["sha256"]="0"*64
        (target/SIDECAR).write_text(json.dumps(data,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    moved=root/"搬移目录";moved.mkdir()
    (root/"analysis256").rename(moved/"analysis256")
    print(f"Examples published below {root}; use inspect_anchor.py to validate the moved, legacy and damaged copies.")


if __name__=="__main__":
    main()
