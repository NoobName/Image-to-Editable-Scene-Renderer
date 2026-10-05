"""Per-region assets and sidecars; all meshes share the same photograph texture."""
import json
from PIL import Image
from .gltf_writer import write_glb
from .scene_decomposer import split_mesh
from .material_assets import material_glb_options
from .material_estimation_backend import MaterialEstimate


def export_objects(root,image,analysis,geometry):
    segmentation = analysis.segmentation
    if segmentation is None:
        return [{"id":"image-surface","name":"Reconstructed Image Surface","mesh":"meshes/scene_mesh.glb"}],{}
    meshes,stats = split_mesh(geometry,segmentation)
    objects,report = [],[]
    for region in segmentation.regions:
        directory = root/"objects"/region.object_id
        directory.mkdir(parents=True)
        mask_path = f"objects/{region.object_id}/mask.png"
        metadata_path = f"objects/{region.object_id}/region.json"
        Image.fromarray(region.mask.astype("uint8")*255).save(root/mask_path)
        mesh = meshes[region.object_id]
        item = {"id":region.object_id,"name":region.name,"region":metadata_path}
        if mesh is not None:
            if len(segmentation.regions)==1 and not stats["boundary_triangles_removed"]:
                item["mesh"] = "meshes/scene_mesh.glb"
            else:
                item["mesh"] = f"objects/{region.object_id}/mesh.glb"
                color = "object_albedo.png" if isinstance(analysis.material, MaterialEstimate) else "base_color.png"
                write_glb(root/item["mesh"],mesh, base_color_uri="../../textures/"+color,name=region.name,
                          **material_glb_options(root, analysis.material, False))
        metadata = {"id":region.object_id,"name":region.name,"labelId":region.label_id,"category":region.category,
                    "mask":mask_path,"imageSize":[image.width,image.height],"boundingBox":list(region.bounding_box),
                    "pixelCount":int(region.mask.sum()),"validDepthPixels":region.valid_depth_pixels,
                    "averageDepth":region.average_depth,"namingSource":region.naming_source,"score":region.score,
                    "triangleCount":0 if mesh is None else len(mesh.indices)}
        (root/metadata_path).write_text(json.dumps(metadata,indent=2,ensure_ascii=False,allow_nan=False)+"\n",encoding="utf-8")
        objects.append(item)
        report.append({**metadata,"vertices":0 if mesh is None else len(mesh.positions)})
    return objects,{**stats,"regions":report,"backend":segmentation.metadata}
