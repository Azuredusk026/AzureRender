"""Restore the pinned probe's secondary UV stream on imported triangle corners."""
import hashlib
from pathlib import Path

import numpy as np
from endfield_native.mesh_uvs import match_uv_triangles


def restore_uv1(meshes, exported, probe):
    if exported.get('Uv0Components') != 2 or exported.get('Uv1Components') != 2:
        raise ValueError('Independent mesh baking requires two native float2 UV streams')
    count = exported['VertexCount']
    paths = {}
    for key in ('File','Uv0File','Uv1File'):
        path = (Path(probe)/exported[key].replace('\\','/')).resolve(strict=True)
        if not path.is_relative_to(Path(probe).resolve()):
            raise ValueError('Mesh UV stream is outside its source probe')
        paths[key] = path
    positions, triangles = [], []
    for line in paths['File'].read_text(encoding='utf-8').splitlines():
        if line.startswith('v '):
            x,y,z = map(float,line.split()[1:4])
            # The pinned probe stores reflected X; Blender uses Z-up.
            positions.append([-x,-z,y])
        elif line.startswith('f '):
            values = [part.split('/') for part in line.split()[1:]]
            if len(values) != 3 or any(len(v)<2 or v[0]!=v[1] for v in values):
                raise ValueError('Native OBJ UV matching requires aligned triangle indices')
            triangles.append([int(v[0])-1 for v in values])
    if len(positions) != count:
        raise ValueError('Native OBJ vertex count differs from its UV stream')
    uv0,uv1 = (np.fromfile(paths[key],dtype='<f4') for key in ('Uv0File','Uv1File'))
    if len(uv0) != count*2 or len(uv1) != count*2:
        raise ValueError('Native UV stream byte count differs from its vertex count')
    uv0,uv1 = uv0.reshape(-1,2),uv1.reshape(-1,2)
    audits = []
    for mesh in meshes:
        if not mesh.uv_layers or any(p.loop_total!=3 for p in mesh.polygons):
            raise ValueError('Imported native mesh requires primary UV and triangle faces')
        primary = mesh.uv_layers[0]
        corners = [list(mesh.vertices[l.vertex_index].co) for l in mesh.loops]
        coordinates = [list(v.uv) for v in primary.data]
        values,audit = match_uv_triangles(positions,uv0,uv1,triangles,corners,coordinates)
        primary.name = 'UV0'
        target = mesh.uv_layers.get('UV1') or mesh.uv_layers.new(name='UV1')
        for item,value in zip(target.data,values):
            item.uv=value
        audits.append(dict(mesh=mesh.name,**audit))
    return dict(contract='NativeMeshUvStreams/1',meshes=audits,
                inputs={key:dict(path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                        for key,path in paths.items()})
