"""Rebuild with recorded filters; material estimation must not silently alter geometry."""
from .geometry_builder import GeometryBuilder


def rebuild_geometry(image, analysis, report):
    settings = report.get("geometry", {}).get("mesh_filtering", {})
    limits = settings.get("thresholds", {})
    grid = max(settings["sampled_grid"]) if settings.get("mode") == "coarse" else None
    builder = GeometryBuilder(grid_size=grid, confidence_threshold=limits.get("confidence", .5),
        depth_edge_threshold=limits.get("relative_depth", .15), depth_edge_meters=limits.get("absolute_depth_meters", 0),
        max_edge_stretch=limits.get("max_edge_stretch", 8), max_edge_meters=limits.get("max_edge_meters", 0),
        max_triangle_area=limits.get("max_triangle_area_m2", 0))
    mesh = builder.build(image, analysis)
    for key, actual in (("vertices", len(mesh.positions)), ("triangles", len(mesh.indices))):
        if report.get("geometry", {}).get(key, actual) != actual:
            raise ValueError("Saved mesh filters cannot reproduce geometry; remesh this legacy package first")
    return mesh
