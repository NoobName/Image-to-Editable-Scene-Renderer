"""Source/analysis diagnostics only. Preview resampling never changes exported source assets."""
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw


def write_diagnostic(root, metadata, output=None):
    root = Path(root)
    output = Path(output) if output else root / "debug/source-analysis.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    source, analysis = metadata["sourceImage"], metadata["analysisImage"]
    sw, sh = source["size"]
    aw, ah = analysis["size"]
    sx, sy = aw / sw, ah / sh
    samples = []
    for x, y in ((.5,.5), (sw/2,sh/2), (sw-.5,sh-.5)):
        ax, ay = sx*x, sy*y
        samples.append({"source": [x,y], "analysis": [ax,ay], "roundtripError": max(abs(ax/sx-x),abs(ay/sy-y))})
    report = {"sourceKind": metadata["sourceKind"], "sourceId": metadata["sourceId"],
        "sourceSize": source["size"], "analysisSize": analysis["size"],
        "sourceSha256": source["sha256"], "analysisSha256": analysis["sha256"],
        "sourceToAnalysis": metadata["analysisMapping"]["sourceToAnalysis"],
        "pixelConvention": metadata["analysisMapping"]["pixelConvention"], "samples": samples,
        "mappingValid": all(math.isfinite(s["roundtripError"]) and s["roundtripError"] < 1e-9 for s in samples),
        "capabilities": metadata["capabilities"], "sourceCameraStatus": metadata["sourceCamera"]["status"]}
    canvas = Image.new("RGB", (1500,860), (23,29,37))
    draw = ImageDraw.Draw(canvas)
    for index, (name, record) in enumerate((("SOURCE ANCHOR",source),("ANALYSIS / processed original_image.png",analysis))):
        x = 20 + index*750
        draw.text((x,16), name, fill="white")
        draw.text((x,38), f'{record["size"][0]} x {record["size"][1]} | sRGB RGB8 | sha256 {record["sha256"][:20]}', fill=(180,208,225))
        with Image.open(root / record["path"]) as opened:
            image = opened.convert("RGB")
            scale = min(710/image.width,490/image.height)
            preview = image.resize((round(image.width*scale),round(image.height*scale)),Image.Resampling.LANCZOS)
            canvas.paste(preview,(x,65))
            # Same source-space crop: analysis is magnified for inspection, never called high-resolution data.
            box = (0,0,min(sw,500),min(sh,170))
            if index:
                box = (0,0,max(1,round(box[2]*sx)),max(1,round(box[3]*sy)))
            crop = image.crop(box);crop = crop.resize((710,220),Image.Resampling.NEAREST)
            canvas.paste(crop,(x,590))
        draw.text((x,566), "Matching top-left detail crop (display magnification only)", fill="white")
    draw.text((20,828), f'{metadata["sourceKind"]} | pixel centers (0.5,0.5) | scales {sx:.9f}, {sy:.9f} | mapping valid: {report["mappingValid"]}',fill=(240,207,112))
    canvas.save(output)
    output.with_suffix(".json").write_text(json.dumps(report,indent=2,ensure_ascii=False,allow_nan=False)+"\n",encoding="utf-8")
    return report
