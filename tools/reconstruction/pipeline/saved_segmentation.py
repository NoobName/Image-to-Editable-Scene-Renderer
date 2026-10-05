"""Restore saved exclusive masks when remeshing; no segmentation inference."""
from pathlib import Path
import numpy as np
from PIL import Image
from .segmentation_types import ObjectRegion,SegmentationResult
try:
    from ..scene_package import load_package,load_region,asset_path
except ImportError:
    from scene_package import load_package,load_region,asset_path


def load_saved_segmentation(package,image,geometry):
    root = Path(package)
    manifest = load_package(root)
    objects = manifest["objects"]
    if not any("region" in obj for obj in objects):
        return None
    if not all("region" in obj for obj in objects):
        raise ValueError("Remeshing segmented packages requires region metadata for every object")
    labels = np.zeros((image.height,image.width),np.uint32)
    regions = []
    for obj in objects:
        r = load_region(root,obj)
        with Image.open(asset_path(root,r["mask"],"objects",(".png",))) as stored:
            pixels = np.asarray(stored)
        if pixels.shape!=labels.shape or not np.isin(pixels,[0,255]).all():
            raise ValueError("Saved region mask must be a same-size binary grayscale PNG")
        mask = pixels>0
        if (mask & (labels>0)).any() or int(mask.sum())!=r["pixelCount"]:
            raise ValueError("Saved masks overlap or disagree with metadata")
        y,x = np.nonzero(mask)
        bbox = (int(x.min()),int(y.min()),int(x.max()-x.min()+1),int(y.max()-y.min()+1))
        if list(bbox)!=r["boundingBox"]:
            raise ValueError("Saved mask bounding box disagrees with metadata")
        labels[mask] = r["labelId"]
        valid = mask & geometry.valid_mask
        average = float(geometry.depth[valid].astype(np.float64).mean()) if valid.any() else None
        regions.append(ObjectRegion(r["id"],r["labelId"],r["name"],r["category"],mask,bbox,average,int(valid.sum()),r["namingSource"],r["score"]))
    with Image.open(root/"masks/segmentation.png") as packed:
        rgb = np.array(packed.convert("RGB"),dtype=np.uint32)
    if not np.array_equal(labels,rgb[:,:,0]+(rgb[:,:,1]<<8)+(rgb[:,:,2]<<16)):
        raise ValueError("Saved per-object masks disagree with segmentation label map")
    return SegmentationResult(labels,tuple(regions),{"restored_from_package":str(root)})
