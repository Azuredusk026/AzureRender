"""Convert captured smooth directions into an imported orthonormal tangent frame."""
import numpy as np


def _array(values,width,label):
    result=np.asarray(values,dtype=float)
    if result.ndim!=2 or result.shape[1]!=width or not len(result) or not np.isfinite(result).all():
        raise ValueError(label+' requires finite, nonempty rows of width '+str(width))
    return result


def _unit(values):
    length=np.linalg.norm(values,axis=1,keepdims=True)
    if np.any(length<=1e-12):raise ValueError('Direction frame contains a degenerate vector')
    return values/length


def encode_octahedral(directions):
    direction=_unit(_array(directions,3,'Directions'))
    projection=direction/np.abs(direction).sum(1,keepdims=True)
    xy=projection[:,:2].copy();back=projection[:,2]<0
    xy[back]=(1-np.abs(xy[back,::-1]))*np.where(xy[back]>=0,1,-1)
    return xy.astype(np.float32)


def decode_octahedral(coordinates):
    xy=_array(coordinates,2,'Octahedral coordinates')
    if np.any(np.abs(xy)>1):raise ValueError('Octahedral coordinates require the signed unit range')
    direction=np.column_stack((xy,1-np.abs(xy).sum(1)))
    back=direction[:,2]<0
    direction[back,:2]=(1-np.abs(xy[back,::-1]))*np.where(xy[back]>=0,1,-1)
    return _unit(direction)


def smooth_direction_coordinates(source_normals,source_tangents,source_uv1,
                                 imported_normals,imported_tangents,coordinate_transform=None):
    source_n=_array(source_normals,3,'Source normals');source_t=_array(source_tangents,4,'Source tangents')
    xy=_array(source_uv1,2,'Source smooth coordinates')
    imported_n=_array(imported_normals,3,'Imported normals');imported_t=_array(imported_tangents,4,'Imported tangents')
    if any(len(value)!=len(source_n) for value in [source_t,xy,imported_n,imported_t]):
        raise ValueError('Smooth directions and both tangent frames must have matching row counts')
    if np.any(np.abs(xy)>1):raise ValueError('Source smooth coordinates require the signed unit range')
    if any(np.any(np.abs(np.abs(value[:,3])-1)>1e-6) for value in [source_t,imported_t]):
        raise ValueError('Tangent handedness must be positive or negative one')
    transform=np.eye(3) if coordinate_transform is None else np.asarray(coordinate_transform,dtype=float)
    if transform.shape!=(3,3) or not np.isfinite(transform).all() or not np.allclose(transform.T@transform,np.eye(3),atol=1e-8,rtol=0):
        raise ValueError('Coordinate conversion requires an orthogonal three-dimensional basis')
    normal=_unit(imported_n)
    tangent=_unit(imported_t[:,:3]-(imported_t[:,:3]*normal).sum(1)[:,None]*normal)
    bitangent=np.cross(normal,tangent)*imported_t[:,3,None]
    z=np.sqrt(np.maximum(1-(xy*xy).sum(1),0))
    desired=_unit((source_t[:,:3]*xy[:,0,None]
        +np.cross(source_n,source_t[:,:3])*source_t[:,3,None]*xy[:,1,None]+source_n*z[:,None])@transform.T)
    return _unit(np.column_stack(((desired*tangent).sum(1),(desired*bitangent).sum(1),(desired*normal).sum(1))))
