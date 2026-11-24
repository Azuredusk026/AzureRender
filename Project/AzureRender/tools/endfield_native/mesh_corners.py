"""Match imported triangle corners to finite native attribute streams."""
from itertools import product,permutations
import numpy as np


def _stream(values,width=None):
    array=np.asarray(values,dtype=np.float64)
    if (array.ndim!=2 or len(array)==0 or array.shape[1]==0
            or (width is not None and array.shape[1]!=width) or not np.isfinite(array).all()):
        raise ValueError('Mesh source streams must be nonempty finite arrays')
    return array


def match_triangle_corners(source_positions,source_uv0,source_attributes,source_triangles,
                           corner_positions,corner_uv0,*,orientation='any'):
    if orientation not in ('any','same','opposite'):
        raise ValueError('Unknown native corner winding orientation')
    positions,uv0,attributes=_stream(source_positions,3),_stream(source_uv0,2),_stream(source_attributes)
    corners,coordinates=_stream(corner_positions,3),_stream(corner_uv0,2)
    if len(positions)!=len(uv0) or len(positions)!=len(attributes) or len(corners)!=len(coordinates):
        raise ValueError('Mesh source stream lengths differ')
    indices=np.asarray(source_triangles)
    if (indices.ndim!=2 or indices.shape[1]!=3 or len(indices)==0 or indices.dtype.kind not in 'iu'
            or indices.min()<0 or indices.max()>=len(positions) or len(corners)%3):
        raise ValueError('Native corner source requires valid triangle indices')
    cell_size,position_tolerance,uv_tolerance=1e-4,3e-6,1e-6
    face_positions,face_uv0,face_attributes=positions[indices],uv0[indices],attributes[indices]
    cells={}
    for index,cell in enumerate(np.floor(face_positions.mean(axis=1)/cell_size).astype(np.int64)):
        cells.setdefault(tuple(cell),[]).append(index)
    neighbours=list(product((-1,0,1),repeat=3));orders=list(permutations(range(3)))
    if orientation!='any':
        parity=0 if orientation=='same' else 1
        orders=[order for order in orders if sum(order[a]>order[b] for a in range(3) for b in range(a+1,3))%2==parity]
    result=np.empty((len(corners),attributes.shape[1]),dtype=np.float64)
    maximum_position_error,maximum_uv_error=0.,0.
    for index in range(0,len(corners),3):
        position,coordinate=corners[index:index+3],coordinates[index:index+3]
        cell=np.floor(position.mean(axis=0)/cell_size).astype(np.int64)
        candidates=[item for offset in neighbours for item in cells.get(tuple(cell+offset),())]
        values=[]
        for candidate in candidates:
            for order in orders:
                order=list(order)
                position_error=float(np.max(abs(face_positions[candidate,order]-position)))
                uv_error=float(np.max(abs(face_uv0[candidate,order]-coordinate)))
                if position_error<=position_tolerance and uv_error<=uv_tolerance:
                    values.append(face_attributes[candidate,order])
                    maximum_position_error=max(maximum_position_error,position_error)
                    maximum_uv_error=max(maximum_uv_error,uv_error)
        if not values:
            raise ValueError(f'no matching native corner source for mesh triangle {index//3}')
        if np.max(abs(np.asarray(values)-values[0]))>uv_tolerance:
            raise ValueError(f'ambiguous source attribute seam at mesh triangle {index//3}')
        result[index:index+3]=values[0]
    return result,dict(sourceVertices=len(positions),corners=len(corners),
        maximumPositionError=maximum_position_error,maximumUv0Error=maximum_uv_error)
