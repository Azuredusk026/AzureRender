"""Strict, lossless material-preserving single-clip GLB assembly."""
import argparse
import copy
import hashlib
import json
import struct
from pathlib import Path
import numpy as np
from retarget_animation import read_glb, trs_matrix


def accessor(doc, binary, index):
    a = doc['accessors'][index]
    if 'sparse' in a: raise ValueError('Sparse accessor requires explicit expansion')
    sizes = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}
    types = {5126: '<f4', 5125: '<u4', 5123: '<u2', 5121: 'u1'}
    width = sizes[a['type']]; dtype = np.dtype(types[a['componentType']])
    view = doc['bufferViews'][a['bufferView']]
    if view.get('buffer', 0) != 0: raise ValueError('External buffer unsupported')
    start = view.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = view.get('byteStride', width * dtype.itemsize)
    end = start + max(0, a['count'] - 1) * stride + width * dtype.itemsize
    if end > len(binary) or end > view.get('byteOffset', 0) + view['byteLength']:
        raise ValueError('Accessor exceeds buffer view')
    values = np.ndarray((a['count'], width), dtype=dtype, buffer=binary, offset=start,
                        strides=(stride, dtype.itemsize)).copy()
    if a.get('normalized'):
        if a['componentType'] not in (5121, 5123): raise ValueError('Unsupported normalized accessor type')
        values = values.astype(np.float64) / np.iinfo(dtype).max
    if not np.isfinite(values).all(): raise ValueError('Nonfinite accessor')
    return values


def skeleton(doc, binary):
    nodes = doc['nodes']; parents = {}
    for i, node in enumerate(nodes):
        for child in node.get('children', []):
            if child in parents: raise ValueError('Multiple node parents')
            parents[child] = i
    paths = {}; active = set()
    def path(i):
        if i in paths: return paths[i]
        if i in active: raise ValueError('Cyclic skeleton')
        active.add(i)
        name = nodes[i].get('name') or ('@node:' + str(i))
        result = (path(parents[i]) + '/' if i in parents else '') + name
        active.remove(i); paths[i] = result
        return result
    for i in range(len(nodes)): path(i)
    if len(set(paths.values())) != len(paths): raise ValueError('Ambiguous node paths')
    records = []
    for i, node in enumerate(nodes):
        matrix = np.array(node['matrix']).reshape(4,4).T if 'matrix' in node else trs_matrix([
            node.get('translation', [0,0,0]), node.get('rotation', [0,0,0,1]), node.get('scale', [1,1,1])])
        records.append([paths[i], np.round(matrix, 7).tolist()])
    skins = []
    for skin in doc.get('skins', []):
        binds = accessor(doc, binary, skin['inverseBindMatrices']).tolist() if 'inverseBindMatrices' in skin else [np.eye(4).flatten().tolist()] * len(skin['joints'])
        skins.append(sorted([[paths[j], np.round(bind, 7).tolist()] for j, bind in zip(skin['joints'], binds)]))
    payload = json.dumps([sorted(records), skins], separators=(',', ':')).encode()
    return hashlib.sha256(payload).hexdigest(), paths


def match_skeletons(target, target_binary, source, source_binary):
    a, ap = skeleton(target, target_binary); b, bp = skeleton(source, source_binary)
    if a != b: raise ValueError(f'Skeleton fingerprint mismatch: {a} != {b}')
    reverse = {p: i for i,p in ap.items()}
    return {i: reverse[p] for i,p in bp.items()}


def select_clip(doc, name):
    matches = [a for a in doc.get('animations', []) if a.get('name') == name]
    if len(matches) != 1 or not matches[0].get('channels'): raise ValueError('Expected one nonempty clip: ' + name)
    return matches[0]


def reference_times(duration, rate=60):
    return sorted(set([duration * i / 4 for i in range(5)] +
                      [min(duration, duration * i / 4 + .5 / rate) for i in range(5)]))


def resample(times, values, query, rotation=False, interpolation='LINEAR'):
    times, values = np.asarray(times), np.asarray(values,dtype=float)
    if interpolation not in ('LINEAR','STEP'): raise ValueError('Bake requires solved LINEAR or STEP curves')
    result=[]
    for t in query:
        i=max(0,min(len(times)-2,int(np.searchsorted(times,t,side='right')-1)))
        alpha=float(np.clip((t-times[i])/(times[i+1]-times[i]),0,1))
        a,b=values[i].copy(),values[i+1].copy()
        if interpolation=='STEP': value=b if alpha>=1 else a
        elif rotation:
            a/=np.linalg.norm(a); b/=np.linalg.norm(b); dot=float(np.dot(a,b))
            if dot<0: b=-b; dot=-dot
            if dot>.9995: value=a*(1-alpha)+b*alpha
            else:
                theta=np.arccos(np.clip(dot,-1,1))
                value=(np.sin((1-alpha)*theta)*a+np.sin(alpha*theta)*b)/np.sin(theta)
            value/=np.linalg.norm(value)
        else: value=a*(1-alpha)+b*alpha
        result.append(value)
    return np.asarray(result)


def compatible_geometry(target, tb, source, sb):
    tm, sm = target.get('meshes', []), source.get('meshes', [])
    if len(tm) != len(sm): raise ValueError('Mesh topology mismatch')
    pairs = []
    for a,b in zip(tm,sm):
        if len(a['primitives']) != len(b['primitives']): raise ValueError('Primitive topology mismatch')
        for x,y in zip(a['primitives'],b['primitives']):
            if x.get('mode',4) != y.get('mode',4): raise ValueError('Primitive topology mode mismatch')
            for key in ('POSITION',):
                if not np.array_equal(accessor(target,tb,x['attributes'][key]),accessor(source,sb,y['attributes'][key])):
                    raise ValueError('Vertex topology mismatch')
            if ('indices' in x) != ('indices' in y): raise ValueError('Index topology mismatch')
            if 'indices' in x and not np.array_equal(accessor(target,tb,x['indices']),accessor(source,sb,y['indices'])):
                raise ValueError('Index topology mismatch')
            pairs.append((x,y))
    return pairs


def assemble(target, source, clip_name, output, source_kind='retargeted', restore_weights=False, bake_rate=60):
    target, source, output = Path(target), Path(source), Path(output)
    if source_kind not in ('retargeted', 'procedural', 'authored'): raise ValueError('Native provenance requires a verified source manifest')
    if output.exists(): raise FileExistsError(output)
    td, tc = read_glb(target); sd, sc = read_glb(source)
    tb = next(data for kind,data in tc if kind == 0x004e4942)
    sb = next(data for kind,data in sc if kind == 0x004e4942)
    mapping = match_skeletons(td, tb, sd, sb); clip = copy.deepcopy(select_clip(sd, clip_name))
    binary = bytearray(tb); maximum = 0.; copied = {}
    def transfer(index, animation=True):
        if index in copied: return copied[index]
        old = sd['accessors'][index]; values = accessor(sd, sb, index)
        if animation and old['componentType'] != 5126: raise ValueError('Animation accessors require floats')
        binary.extend(b'\0' * (-len(binary) % 4)); start = len(binary)
        dtype = '<f4' if old.get('normalized') else {5126:'<f4',5123:'<u2',5121:'u1',5125:'<u4'}[old['componentType']]
        raw = values.astype(dtype).tobytes(); binary.extend(raw)
        views = td.setdefault('bufferViews', []); vi = len(views)
        views.append({'buffer': 0, 'byteOffset': start, 'byteLength': len(raw)})
        new = copy.deepcopy(old); new['bufferView'] = vi; new['byteOffset'] = 0
        if old.get('normalized'): new['componentType'] = 5126; new.pop('normalized',None)
        accessors = td.setdefault('accessors', []); copied[index] = len(accessors); accessors.append(new)
        return copied[index]
    if restore_weights:
        for target_primitive, source_primitive in compatible_geometry(td,tb,sd,sb):
            for key in ('JOINTS_0','WEIGHTS_0'):
                target_primitive['attributes'][key] = transfer(source_primitive['attributes'][key],animation=False)
    def emit(values, kind):
        values=np.asarray(values,dtype='<f4'); binary.extend(b'\0'*(-len(binary)%4)); start=len(binary)
        raw=values.tobytes(); binary.extend(raw); views=td.setdefault('bufferViews',[])
        view=len(views); views.append({'buffer':0,'byteOffset':start,'byteLength':len(raw)})
        item={'bufferView':view,'componentType':5126,'count':len(values),'type':kind}
        if kind=='SCALAR': item.update(min=[float(values.min())],max=[float(values.max())])
        index=len(td.setdefault('accessors',[])); td['accessors'].append(item); return index
    if bake_rate<=0 or not np.isfinite(bake_rate): raise ValueError('Invalid bake rate')
    for sampler_index,sampler in enumerate(clip['samplers']):
        times = accessor(sd, sb, sampler['input']).flatten()
        if len(times) < 2 or np.any(np.diff(times) <= 0): raise ValueError('Invalid animation time sequence')
        maximum = max(maximum, float(times[-1]))
        paths={c['target']['path'] for c in clip['channels'] if c['sampler']==sampler_index}
        if not paths or not paths <= {'translation','rotation','scale'} or ('rotation' in paths and len(paths)>1):
            raise ValueError('Bake requires a typed TRS sampler')
        query=np.unique(np.r_[np.arange(times[0],times[-1],1/bake_rate),times[-1]])
        if sampler.get('interpolation')=='STEP': query=np.unique(np.r_[query,times])
        values=resample(times,accessor(sd,sb,sampler['output']),query,rotation='rotation' in paths,
                        interpolation=sampler.get('interpolation','LINEAR'))
        sampler['input']=emit(query,'SCALAR'); sampler['output']=emit(values,'VEC4' if 'rotation' in paths else 'VEC3')
        sampler['interpolation']='STEP' if sampler.get('interpolation')=='STEP' else 'LINEAR'
    for channel in clip['channels']:
        channel['target']['node'] = mapping[channel['target']['node']]
    td['animations'] = [clip]; td['buffers'] = [{'byteLength': len(binary)}]
    payload = json.dumps(td, ensure_ascii=False, separators=(',', ':')).encode(); payload += b' ' * (-len(payload) % 4)
    binary.extend(b'\0' * (-len(binary) % 4))
    result = struct.pack('<III', 0x46546c67, 2, 28 + len(payload) + len(binary))
    result += struct.pack('<II', len(payload), 0x4e4f534a) + payload
    result += struct.pack('<II', len(binary), 0x004e4942) + binary
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as stream: stream.write(result)
    return {'schemaVersion': 1, 'sourceKind': source_kind, 'clip': clip_name,
            'sources': [{'path': str(p.resolve()), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in [target, source]],
            'outputSha256': hashlib.sha256(result).hexdigest(), 'skeletonFingerprint': skeleton(td, binary)[0],
            'duration': maximum, 'referenceTimes': reference_times(maximum),
            'materialCount': len(td.get('materials', [])), 'materialsPreserved': td.get('materials') == read_glb(target)[0].get('materials'),
            'weightRestoration': 'animation source, exact POSITION and topology correspondence' if restore_weights else 'target preserved',
            'sampling': {'sampleRate':bake_rate,'rotation':'shortest-path SLERP','endpoint':'preserved'},
            'nativeSourceStatus': 'pending verified game clip and Avatar'}


if __name__ == '__main__':
    p = argparse.ArgumentParser(__doc__)
    for flag in ('target', 'source', 'output'): p.add_argument('--' + flag, type=Path, required=True)
    p.add_argument('--clip', required=True)
    p.add_argument('--restore-weights', action='store_true')
    args = p.parse_args(); report = assemble(args.target, args.source, args.clip, args.output,restore_weights=args.restore_weights)
    args.output.with_suffix('.manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
