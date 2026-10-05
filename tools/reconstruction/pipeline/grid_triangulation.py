"""Bounded-memory grid triangulation in LH camera space; no neural network dependency."""
import numpy as np


def _integral(mask):
    return np.pad(mask.astype(np.int32), ((1, 0), (1, 0))).cumsum(0).cumsum(1)


def _rectangle(sums, x0, y0, x1, y1):
    return sums[y1, x1]-sums[y0, x1]-sums[y1, x0]+sums[y0, x0]


def triangulate(prediction, *, grid_size, confidence_threshold, depth_edge_threshold,
                depth_edge_meters, max_edge_stretch, max_edge_meters, max_triangle_area):
    """Keep every eligible sampled vertex, including isolated ones; emit only safe faces.

    Pixel mode is the default. Coarse mode is explicit and conservatively rejects cells
    containing invalid pixels or depth discontinuities anywhere in their image rectangle.
    Each cell chooses the diagonal retaining most valid triangles, then the shorter
    3D diagonal. All four candidates use the same checks, including diagonal edges.
    """
    height, width = prediction.depth.shape
    if min(height, width) < 2:
        raise ValueError("Pixel triangulation requires at least 2 x 2 pixels; a single row has no surface")
    if max(height, width) > 2048:
        raise ValueError("Mesh input exceeds 2048 pixels per edge; resize prediction before meshing")
    columns = width if grid_size is None else min(width, max(2, round((grid_size-1)*width/max(width, height))+1))
    rows = height if grid_size is None else min(height, max(2, round((grid_size-1)*height/max(width, height))+1))
    xs = np.rint(np.linspace(0, width-1, columns)).astype(np.int32)
    ys = np.rint(np.linspace(0, height-1, rows)).astype(np.int32)
    x, y = np.meshgrid(xs, ys)
    uv = np.stack(((x+.5)/width, (y+.5)/height), -1).reshape(-1, 2).astype(np.float32)
    points = prediction.point_map[y, x].reshape(-1, 3)
    normals = prediction.normal[y, x].reshape(-1, 3)
    valid_image = prediction.valid_mask & (prediction.confidence >= confidence_threshold)
    valid = valid_image[y, x].ravel()
    fx, fy = prediction.camera_intrinsics[0, 0], prediction.camera_intrinsics[1, 1]

    def depth_ok(z0, z1):
        delta = np.abs(z0-z1)
        ok = delta <= depth_edge_threshold*np.minimum(z0, z1)
        if depth_edge_meters > 0:
            ok &= delta <= depth_edge_meters
        return ok

    coarse = columns != width or rows != height
    if coarse:
        holes = _integral(~valid_image)
        z = prediction.depth
        horizontal = _integral(~depth_ok(z[:, :-1], z[:, 1:]))
        vertical = _integral(~depth_ok(z[:-1], z[1:]))

    counts = dict(invalid=0, depth=0, large_edge=0, large_area=0, degenerate=0)
    chunks = []
    # At most 64 cell rows are expanded into four candidate triangles at once.
    for start in range(0, rows-1, 64):
        stop = min(rows-1, start+64)
        a = (np.arange(start, stop, dtype=np.uint32)[:, None]*columns + np.arange(columns-1, dtype=np.uint32)).ravel()
        b, c, d = a+1, a+columns, a+columns+1
        candidates = np.stack((np.stack((a,b,c), -1), np.stack((b,d,c), -1),
                               np.stack((a,b,d), -1), np.stack((a,d,c), -1)), 1)
        p = points[candidates]
        z = p[..., 2]
        valid_faces = valid[candidates].all(-1)
        depth_faces = depth_ok(z.min(-1), z.max(-1))
        if coarse:
            x0, y0, x1, y1 = x.ravel()[a], y.ravel()[a], x.ravel()[d], y.ravel()[d]
            valid_faces &= (_rectangle(holes, x0,y0,x1+1,y1+1) == 0)[:, None]
            interior_ok = ((_rectangle(horizontal,x0,y0,x1,y1+1) == 0) &
                           (_rectangle(vertical,x0,y0,x1+1,y1) == 0))
            depth_faces &= interior_ok[:, None]
        large_ok = np.ones_like(valid_faces)
        for first, second in ((0,1), (1,2), (2,0)):
            edge = p[:,:,second]-p[:,:,first]
            length_sq = np.einsum('...i,...i->...', edge, edge)
            delta_uv = uv[candidates[:,:,second]]-uv[candidates[:,:,first]]
            # Expected edge length on a front-facing patch at the nearer endpoint.
            footprint_sq = ((delta_uv[...,0]/fx)**2+(delta_uv[...,1]/fy)**2)*np.minimum(z[:,:,first],z[:,:,second])**2
            large_ok &= length_sq <= footprint_sq * max_edge_stretch**2
            if max_edge_meters > 0:
                large_ok &= length_sq <= max_edge_meters**2
        cross = np.cross(p[:,:,1]-p[:,:,0], p[:,:,2]-p[:,:,0])
        double_area = np.linalg.norm(cross, axis=-1)
        area_ok = double_area <= 2*max_triangle_area if max_triangle_area > 0 else np.ones_like(valid_faces)
        nondegenerate = double_area > 1e-12
        checks = [valid_faces, depth_faces, large_ok, area_ok, nondegenerate]
        keep = np.logical_and.reduce(checks)
        score_bc, score_ad = keep[:,:2].sum(1), keep[:,2:].sum(1)
        length_bc = np.sum((points[b]-points[c])**2, axis=1)
        length_ad = np.sum((points[a]-points[d])**2, axis=1)
        use_ad = (score_ad > score_bc) | ((score_ad == score_bc) & (length_ad < length_bc))
        choice = np.stack((use_ad.astype(int)*2, use_ad.astype(int)*2+1), 1)
        cell = np.arange(len(a))[:,None]
        chosen = candidates[cell, choice]
        active = np.ones((len(a),2), dtype=bool)
        for name, check in zip(counts, checks):
            good = check[cell, choice]
            counts[name] += int(np.count_nonzero(active & ~good))
            active &= good
        chunks.append(chosen[active])
    indices = np.concatenate(chunks)
    if not len(indices):
        raise ValueError("No valid triangles after mesh filtering; inspect masks, thresholds and point-map geometry")
    remap = np.full(len(points), -1, dtype=np.int32)
    remap[valid] = np.arange(np.count_nonzero(valid), dtype=np.int32)
    indices = remap[indices].astype(np.uint32)
    diagnostics = {
        "mode": "pixel" if grid_size is None else "coarse", "sampled_grid": [columns, rows],
        "eligible_pixels": int(valid_image.sum()), "candidate_triangles": 2*(rows-1)*(columns-1),
        "rejected": counts, "isolated_vertices": int(valid.sum())-len(np.unique(indices)),
        "thresholds": {"confidence": confidence_threshold, "relative_depth": depth_edge_threshold,
                       "absolute_depth_meters": depth_edge_meters, "max_edge_stretch": max_edge_stretch,
                       "max_edge_meters": max_edge_meters, "max_triangle_area_m2": max_triangle_area}}
    return points[valid], normals[valid], uv[valid], indices, diagnostics
