"""Resolve overlapping masks and split the existing mesh without moving its vertices."""
from dataclasses import replace
import hashlib
import re
import numpy as np
from .segmentation_types import CATEGORIES, MaskProposal, ObjectRegion, SegmentationResult


def validate_proposals(image, prediction):
    if len(prediction.proposals)>4096:
        raise ValueError("Too many segmentation proposals")
    explicit_ids = set()
    for item in prediction.proposals:
        if not isinstance(item.mask,np.ndarray) or item.mask.dtype!=np.bool_ or item.mask.shape!=(image.height,image.width):
            raise ValueError("Segmentation mask must be bool with processed image dimensions")
        if not np.isfinite(item.score) or not 0<=item.score<=1:
            raise ValueError("Segmentation score must be finite in [0,1]")
        if item.category is not None and item.category not in CATEGORIES:
            raise ValueError("Unknown segmentation category")
        if item.name is not None and (not item.name.strip() or len(item.name)>128):
            raise ValueError("Region name must contain 1..128 characters")
        if item.object_id is not None:
            if not re.fullmatch(r"[a-zA-Z0-9_-]{1,64}",item.object_id) or item.object_id in explicit_ids:
                raise ValueError("Prompt IDs must be unique safe identifiers (1..64 letters/digits/_/-)")
            explicit_ids.add(item.object_id)


def _category(mask, geometry):
    y,x = np.nonzero(mask)
    valid = mask & geometry.valid_mask
    if valid.sum() < .05*mask.sum() and y.min()==0 and y.mean()<mask.shape[0]*.4:
        return "sky"
    if (valid.any() and mask.mean()>.05 and x.max()-x.min()>mask.shape[1]*.4 and y.max()>mask.shape[0]*.95
            and (geometry.normal[valid,1]>.65).mean()>.65 and y.mean()>mask.shape[0]*.55):
        return "ground"
    if valid.any() and np.median(geometry.depth[valid])<np.percentile(geometry.depth[geometry.valid_mask],40):
        return "foreground"
    return "major" if mask.mean()>.04 else "background"


def resolve_regions(image, prediction, geometry, *, min_pixels=64, max_regions=32):
    if not isinstance(min_pixels,int) or min_pixels<1 or not isinstance(max_regions,int) or not 2<=max_regions<=128:
        raise ValueError("min-region-pixels must be positive; max-regions must be in [2,128]")
    validate_proposals(image,prediction)
    if prediction.metadata.get("prompted") and len(prediction.proposals)>max_regions-1:
        raise ValueError("Increase --max-regions to include every named prompt plus remaining Background")
    proposals = list(prediction.proposals)
    if not prediction.metadata.get("prompted") and not prediction.metadata.get("dummy"):
        # Small masks win over containing masks. Quality and content break ties deterministically.
        proposals.sort(key=lambda p:(int(p.mask.sum()),-p.score,hashlib.sha256(np.packbits(p.mask).tobytes()).hexdigest()))
    labels = np.zeros((image.height,image.width),np.uint32)
    regions, used_ids, used_names = [], set(), set()
    dropped = 0

    def append(item,mask,fallback=False):
        digest = hashlib.sha256(np.packbits(item.mask).tobytes()).hexdigest()[:16]
        identity = item.object_id or f"region-{digest}"
        if identity in used_ids:  # Duplicate automatic proposals cannot alias logical objects.
            raise ValueError(f"Duplicate resolved object ID: {identity}")
        category = item.category or _category(mask,geometry)
        source = "fallback" if fallback else "dummy" if prediction.metadata.get("dummy") else "prompt" if item.name else "geometry-heuristic"
        name = item.name or {"sky":"Sky","ground":"Ground","foreground":"Foreground Object",
                             "major":"Major Object","background":"Background"}[category]
        base_name, suffix = name, 2
        while name in used_names:
            name = f"{base_name} {suffix}"
            suffix += 1
        label = len(regions)+1
        labels[mask] = label
        y,x = np.nonzero(mask)
        valid = mask & geometry.valid_mask
        depth = float(geometry.depth[valid].astype(np.float64).mean()) if valid.any() else None
        regions.append(ObjectRegion(identity,label,name,category,mask,
            (int(x.min()),int(y.min()),int(x.max()-x.min()+1),int(y.max()-y.min()+1)),depth,int(valid.sum()),source,float(item.score)))
        used_ids.add(identity)
        used_names.add(name)

    for item in proposals:
        if len(regions)>=max_regions-1:
            dropped += 1
            continue
        available = item.mask & (labels==0)
        # Named prompts intentionally support small selected objects. Empty masks remain errors
        # in the adapter; wholly occluded/overlapping later proposals are recorded as dropped.
        required = 1 if item.object_id or prediction.metadata.get("dummy") else min_pixels
        if available.sum()<required:
            dropped += 1
            continue
        append(item,available)
    remainder = labels==0
    if remainder.any():
        fallback_id = "background-remainder"
        while fallback_id in used_ids:
            fallback_id += "-0"
        append(MaskProposal(remainder,object_id=fallback_id,name="Background",category="background"),remainder,True)
    return SegmentationResult(labels,tuple(regions),{**prediction.metadata,"overlap_policy":"prompt-order" if prediction.metadata.get("prompted") else "small-first",
                              "discarded_proposals":dropped,"min_region_pixels":min_pixels,"max_regions":max_regions})


def split_mesh(geometry, segmentation):
    """Keep only faces whose three sampled pixel labels agree; never bridge object masks.

    Boundary faces are dropped, not assigned to both objects. No duplicates/z-fighting,
    invented depth or texture reprojection. UVs stay in the original full-image atlas.
    """
    h,w = segmentation.labels.shape
    x = np.clip((geometry.texcoords[:,0]*w).astype(np.int32),0,w-1)
    y = np.clip((geometry.texcoords[:,1]*h).astype(np.int32),0,h-1)
    labels = segmentation.labels[y,x]
    face_labels = labels[geometry.indices]
    keep = (face_labels[:,0]==face_labels[:,1]) & (face_labels[:,1]==face_labels[:,2]) & (face_labels[:,0]>0)
    # Coarse faces must not skip over a different interior object. Conservative bbox policy.
    if geometry.diagnostics.get("mode")=="coarse":
        for region in segmentation.regions:
            selected = np.flatnonzero(keep & (face_labels[:,0]==region.label_id))
            if not len(selected):
                continue
            wrong = np.pad((~region.mask).astype(np.int32),((1,0),(1,0))).cumsum(0).cumsum(1)
            tx,ty = x[geometry.indices[selected]],y[geometry.indices[selected]]
            x0,x1,y0,y1 = tx.min(1),tx.max(1)+1,ty.min(1),ty.max(1)+1
            keep[selected] &= (wrong[y1,x1]-wrong[y0,x1]-wrong[y1,x0]+wrong[y0,x0])==0
    meshes = {}
    for region in segmentation.regions:
        faces = geometry.indices[keep & (face_labels[:,0]==region.label_id)]
        if not len(faces):
            meshes[region.object_id] = None  # E.g. Sky has a mask but no valid depth.
            continue
        used,indices = np.unique(faces,return_inverse=True)
        meshes[region.object_id] = replace(geometry,positions=geometry.positions[used],normals=geometry.normals[used],
                                          texcoords=geometry.texcoords[used],indices=indices.reshape(-1,3).astype(np.uint32))
    return meshes, {"boundary_triangles_removed":int((~keep).sum()),"assigned_triangles":int(keep.sum())}
