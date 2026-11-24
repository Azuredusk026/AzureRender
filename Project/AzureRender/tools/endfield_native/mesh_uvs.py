"""Match imported mesh corners to independent native UV streams."""
import numpy as np
from .mesh_corners import match_triangle_corners


def match_uv_triangles(source_positions,source_uv0,source_uv1,source_triangles,
                       corner_positions,corner_uv0):
    values=np.asarray(source_uv1,dtype=np.float64)
    if values.ndim!=2 or values.shape[1]!=2:
        raise ValueError('Secondary UV source requires float2 values')
    return match_triangle_corners(source_positions,source_uv0,values,source_triangles,
                                  corner_positions,corner_uv0)
