"""Attach source tangents using an exact exported vertex correspondence."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import numpy as np


def accessor(document, tail, index):
    item = document['accessors'][index]
    if item.get('sparse'): raise ValueError('Sparse correspondence accessors are unsupported')
    view = document['bufferViews'][item['bufferView']]
    if view.get('buffer', 0) != 0: raise ValueError('Expected embedded buffer zero')
    columns = {'SCALAR':1, 'VEC2':2, 'VEC3':3, 'VEC4':4}[item['type']]
    dtype = np.dtype({5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}[item['componentType']])
    stride = view.get('byteStride', dtype.itemsize*columns)
    return np.ndarray((item['count'], columns), dtype=dtype, buffer=tail,
        offset=8+view.get('byteOffset',0)+item.get('byteOffset',0), strides=(stride,dtype.itemsize))


def require_same_geometry(document, tail, correspondence, correspondence_tail):
    names = [m.get('name') for m in correspondence['meshes']]
    if len(set(names)) != len(names): raise ValueError('Ambiguous correspondence mesh names')
    lookup = {name:index for index,name in enumerate(names)}
    matches = {}
    for index,mesh in enumerate(document['meshes']):
        if mesh.get('name') not in lookup: raise ValueError('Missing correspondence mesh')
        other_index = lookup[mesh['name']];other = correspondence['meshes'][other_index]
        if len(mesh['primitives']) != len(other['primitives']): raise ValueError('Primitive counts differ')
        for primitive_index,(left,right) in enumerate(zip(mesh['primitives'],other['primitives'])):
            if left.get('mode',4) != right.get('mode',4): raise ValueError('Primitive modes differ')
            pairs = [(key,value,right['attributes'].get(key)) for key,value in left['attributes'].items()]
            pairs.append(('indices',left['indices'],right['indices']))
            for key,a,b in pairs:
                if b is None or document['accessors'][a].get('normalized',False) != correspondence['accessors'][b].get('normalized',False):
                    raise ValueError('Accessor semantic differs: '+key)
                if not np.array_equal(accessor(document,tail,a),accessor(correspondence,correspondence_tail,b)):
                    raise ValueError('Correspondence differs: '+mesh['name']+'/'+key)
            matches[index,primitive_index] = other_index,primitive_index
    return matches


def transform_tangents(values, matrix):
    values = np.asarray(values,dtype=np.float64);matrix = np.asarray(matrix,dtype=np.float64)
    if not np.isfinite(values).all() or not np.isfinite(matrix).all() or values.shape[1] != 4:
        raise ValueError('Invalid source tangent data')
    if not np.isin(values[:,3],[-1,1]).all(): raise ValueError('Source tangent sign must be -1 or +1')
    determinant = np.linalg.det(matrix)
    if abs(determinant) < 1e-12: raise ValueError('Singular tangent transform')
    result = values.copy();result[:,:3] = values[:,:3] @ matrix.T
    length = np.linalg.norm(result[:,:3],axis=1,keepdims=True)
    if np.any(length < 1e-12): raise ValueError('Zero source tangent')
    result[:,:3] /= length
    result[:,3] *= 1 if determinant > 0 else -1
    return result.astype('<f4')


def source_vertex_indices(values, count):
    values = np.asarray(values).reshape(-1)
    if (not np.isfinite(values).all() or np.any(values < 0) or np.any(values >= count)
            or np.any(values != np.floor(values))
            or (np.issubdtype(values.dtype,np.floating) and count > 16777216)):
        raise ValueError('Native vertex correspondence must contain exact bounded integers')
    return values.astype('<u4')


def append_tangents(document, tail, streams):
    if len(document.get('buffers',[])) != 1 or len(tail) < 8 or struct.unpack_from('<I',tail,4)[0] != 0x004e4942:
        raise ValueError('Expected one embedded GLB buffer')
    result = copy.deepcopy(document);payload = bytearray(tail[8:])
    for (mesh,primitive),values in streams.items():
        target = result['meshes'][mesh]['primitives'][primitive]
        if 'TANGENT' in target['attributes']: raise ValueError('Input already has authored tangents')
        count = document['accessors'][target['attributes']['POSITION']]['count']
        values = np.asarray(values,dtype='<f4')
        if values.shape != (count,4): raise ValueError('Tangent vertex count differs')
        payload.extend(b'\0'*(-len(payload)%4));offset = len(payload);encoded = values.tobytes();payload.extend(encoded)
        view = len(result['bufferViews'])
        result['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':len(encoded),'target':34962})
        index = len(result['accessors'])
        result['accessors'].append({'bufferView':view,'componentType':5126,'type':'VEC4','count':count})
        target['attributes']['TANGENT'] = index
    result['buffers'][0]['byteLength'] = len(payload)
    return result,struct.pack('<II',len(payload),0x004e4942)+bytes(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('input','correspondence','correspondence-manifest','geometry','output','report'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--verify-only',action='store_true',help='Check an existing output against regenerated source streams')
    args = parser.parse_args()
    if (args.output.exists() != args.verify_only) or args.report.exists() or args.output.resolve()==args.report.resolve():
        raise ValueError('Outputs must be distinct new files')
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
    from convert_material_semantics import read_asset,write_asset
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = json.loads(args.correspondence_manifest.read_text(encoding='utf8'))
    if digest(args.geometry) != manifest['geometry_sha256'] or digest(args.correspondence) != manifest['output_sha256']:
        raise ValueError('Correspondence source fingerprint differs')
    geometry = json.loads(args.geometry.read_text(encoding='utf8'))
    native_path = (args.geometry.parent/geometry['binary_path']).resolve()
    if not native_path.is_relative_to(args.geometry.parent.resolve()): raise ValueError('Geometry binary escapes source root')
    binary = native_path.read_bytes()
    if hashlib.sha256(binary).hexdigest()!=geometry['binary_sha256'] or len(binary)!=geometry['binary_bytes']:
        raise ValueError('Native geometry fingerprint differs')
    document,tail = read_asset(args.input);correspondence,ctail = read_asset(args.correspondence)
    matches = require_same_geometry(document,tail,correspondence,ctail)
    native_meshes = {m['path']:m for m in geometry['meshes']}
    bridge_meshes = {m['name']:m for m in manifest['meshes']}
    streams = {};rows = []
    for (mesh_index,primitive_index),(other_index,other_primitive) in matches.items():
        mesh = document['meshes'][mesh_index];bridge = bridge_meshes[mesh['name']];native = native_meshes[bridge['source_path']]
        attrs = correspondence['meshes'][other_index]['primitives'][other_primitive]['attributes']
        indices = source_vertex_indices(accessor(correspondence,ctail,attrs['_AZURE_SOURCE_VERTEX']),native['vertex_count'])
        spec = native['tangents']
        if spec['dtype']!='float32' or spec['components']!=4 or spec['count']!=native['vertex_count']:
            raise ValueError('Invalid native tangent stream')
        values = np.frombuffer(binary,dtype='<f4',offset=spec['offset'],count=spec['count']*4).reshape(-1,4)
        transform = np.asarray(bridge['vertex_transform'],dtype=np.float64)
        matrix = transform[:3,:3]
        position_spec = native['positions']
        positions = np.frombuffer(binary,dtype='<f4',offset=position_spec['offset'],count=position_spec['count']*3).reshape(-1,3)
        expected = positions[indices] @ matrix.T + transform[:3,3]
        actual = accessor(document,tail,mesh['primitives'][primitive_index]['attributes']['POSITION'])
        position_error = float(np.max(np.abs(expected-actual)))
        if position_error > 2e-6: raise ValueError('Source vertex transform does not reproduce exported positions')
        mapped = transform_tangents(values[indices],matrix);streams[mesh_index,primitive_index] = mapped
        rows.append({'mesh':mesh['name'],'primitive':primitive_index,'source_path':native['path'],
                     'vertices':len(indices),'source_vertices':native['vertex_count'],
                     'source_vertex_indices_sha256':hashlib.sha256(indices.astype('<u4').tobytes()).hexdigest(),
                     'tangent_sha256':hashlib.sha256(mapped.tobytes()).hexdigest(),
                     'maximum_source_position_error':position_error,
                     'maximum_unit_length_error':float(np.max(np.abs(np.linalg.norm(mapped[:,:3],axis=1)-1)))})
    result,newtail = append_tangents(document,tail,streams)
    for key in ('nodes','skins','animations','materials','images','textures'):
        if result.get(key)!=document.get(key): raise AssertionError('Unexpected structure change: '+key)
    if newtail[8:len(tail)]!=tail[8:]: raise AssertionError('Original binary prefix changed')
    if not args.verify_only:
        args.output.parent.mkdir(parents=True,exist_ok=True);write_asset(args.output,result,newtail)
    verified,vtail = read_asset(args.output)
    if verified!=result or vtail!=newtail:raise AssertionError('Output does not reproduce regenerated source correspondence')
    for key,values in streams.items():
        index=verified['meshes'][key[0]]['primitives'][key[1]]['attributes']['TANGENT']
        if not np.array_equal(accessor(verified,vtail,index),values): raise AssertionError('Output tangent bytes differ')
    report = {'input_sha256':digest(args.input),'output_sha256':digest(args.output),'generator_sha256':digest(Path(__file__)),
              'correspondence_manifest_sha256':digest(args.correspondence_manifest),'native_binary_sha256':geometry['binary_sha256'],
              'original_attributes_and_indices_equal':True,'original_binary_prefix_preserved':True,
              'nodes_skins_animations_materials_preserved':True,'meshes':rows,
              'scope':'Source converter tangent XYZ transformed to exported geometry space; source W retained for proper rotations. Native lighting equivalence remains unverified.'}
    args.report.write_text(json.dumps(report,indent=2),encoding='utf8')
    print(json.dumps({'meshes':len(rows),'vertices':sum(row['vertices'] for row in rows),'original_binary_prefix_preserved':True}))


if __name__=='__main__':main()
