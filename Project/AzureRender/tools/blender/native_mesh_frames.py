"""Import native OBJ with exact source axes and authored normal corners."""
from pathlib import Path
import hashlib
import json
import re
import numpy as np

from endfield_native.mesh_corners import match_triangle_corners


def _obj_normal_corners(path):
    positions,uvs,normals,faces=[],[],[],[]
    for line in path.read_text(encoding='utf-8').splitlines():
        parts=line.split()
        if not parts:continue
        if parts[0] in ('v','vn'):
            if len(parts)<4:raise ValueError('Native OBJ requires float3 positions and normals')
            value=list(map(float,parts[1:4]))
            (positions if parts[0]=='v' else normals).append([-value[0],-value[2],value[1]])
        elif parts[0]=='vt':uvs.append(list(map(float,parts[1:3])))
        elif parts[0]=='f':faces.append([part.split('/') for part in parts[1:]])
    if not normals:return None
    if not positions or not faces:
        raise ValueError('Authored native normals require positions and triangle faces')
    for values,width in ((positions,3),(uvs,2),(normals,3)):
        if values is uvs and not values:continue
        array=np.asarray(values,dtype=float)
        if array.ndim!=2 or array.shape[1]!=width or not np.isfinite(array).all():
            raise ValueError('Native OBJ contains malformed or nonfinite streams')
    normals=np.asarray(normals,dtype=float);length=np.linalg.norm(normals,axis=1)
    if np.any(length<1e-12):raise ValueError('Native OBJ has a zero authored normal')
    normals/=length[:,None]
    # Blender geometry and native GPU streams both store float32 values.
    # Canonicalize text before applying the unchanged corner tolerances.
    for values in [positions,uvs]:
        if not values:continue
        coordinates=np.asarray(values,dtype=np.float64)
        if np.max(np.abs(coordinates))>np.finfo(np.float32).max:
            raise ValueError('Native OBJ stream exceeds float32 storage')
        values[:]=coordinates.astype(np.float32).astype(np.float64).tolist()
    keys={};p,u,n,triangles=[],[],[],[]
    for face in faces:
        if len(face)!=3:raise ValueError('Native authored normal matching requires triangle faces')
        triangle=[]
        for corner in face:
            if len(corner)!=3 or not corner[0] or not corner[2] or (uvs and not corner[1]):
                raise ValueError('Native OBJ requires complete authored normal corners')
            indices=(int(corner[0])-1,int(corner[1])-1 if corner[1] else -1,int(corner[2])-1)
            if any(index<0 or index>=count for index,count in ((indices[0],len(positions)),(indices[2],len(normals)))) or (uvs and not 0<=indices[1]<len(uvs)):
                raise ValueError('Native OBJ corner index is out of range')
            if indices not in keys:
                keys[indices]=len(p);p.append(positions[indices[0]]);u.append(uvs[indices[1]] if uvs else [0,0]);n.append(normals[indices[2]])
            triangle.append(keys[indices])
        triangles.append(triangle)
    return p,u,n,triangles,bool(uvs)


def _restore_obj_normals(meshes,path,source):
    audits=[];digest=hashlib.sha256(path.read_bytes()).hexdigest()
    for mesh in meshes:
        if source is None:
            audit=dict(mesh=mesh.name,authoredNormals=False)
        else:
            if (source[4] and not mesh.uv_layers) or any(p.loop_total!=3 for p in mesh.polygons):
                raise ValueError('Authored native normals require imported UVs and triangles')
            values,audit=match_triangle_corners(*source[:4],
                [list(mesh.vertices[l.vertex_index].co) for l in mesh.loops],
                [list(item.uv) for item in mesh.uv_layers[0].data] if source[4] else [[0,0]]*len(mesh.loops),orientation='opposite')
            mesh.normals_split_custom_set(values.tolist());mesh.update()
            audit.update(mesh=mesh.name,authoredNormals=True)
        audit.update(contract='NativeObjCornerNormals/1',source=str(path),sha256=digest)
        mesh['azureNativeNormalReceipt']=json.dumps(audit,sort_keys=True);audits.append(audit)
    return audits


def restore_obj_normals(meshes,path):
    path=Path(path).resolve(strict=True)
    return _restore_obj_normals(meshes,path,_obj_normal_corners(path))


def import_native_obj(path):
    import bpy
    from mathutils import Matrix
    path=Path(path).resolve(strict=True);source=_obj_normal_corners(path)
    before=set(bpy.data.objects)
    try:
        bpy.ops.wm.obj_import(filepath=str(path),forward_axis='NEGATIVE_Z',up_axis='Y',
            use_split_objects=False,use_split_groups=True)
        objects=[obj for obj in set(bpy.data.objects)-before if obj.type=='MESH']
        if not objects:raise ValueError('Native OBJ contains no triangle meshes')
        # Bake the source axis contract as an exact signed permutation.
        # The temporary object rotation is represented through Euler angles.
        basis=Matrix(((-1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))
        for obj in objects:
            obj.data.transform(basis);obj.data.flip_normals();obj.data.update()
            obj.matrix_world=Matrix.Identity(4)
        _restore_obj_normals([obj.data for obj in objects],path,source)
        def group_order(obj):
            match=re.search(r'_(\d+)(?:\.\d+)?$',obj.name)
            return (int(match[1]) if match else 0,obj.name.casefold())
        return sorted(objects,key=group_order)
    except Exception:
        for obj in set(bpy.data.objects)-before:
            mesh=obj.data if obj.type=='MESH' else None;bpy.data.objects.remove(obj,do_unlink=True)
            if mesh is not None and mesh.users==0:bpy.data.meshes.remove(mesh)
        raise
