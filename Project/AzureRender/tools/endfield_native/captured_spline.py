"""Animate captured opaque templates through existing rigid-node GLB curves.

Templates retain their captured orientation, size, UVs and material bytes.
Source spline timing owns motion, inactive gaps and opacity. Template choice,
initial global phase and subsequent random resampling are authored choices.
"""
import copy
import math
import struct


def _number(value, name):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError('invalid ' + name)
    return value


def _vector(value, width, name):
    if not isinstance(value, (list, tuple)) or len(value) != width:
        raise ValueError('invalid ' + name)
    return [_number(v, name) for v in value]


def _rigid_world(value):
    if not isinstance(value, (list, tuple)) or len(value) != 4:
        raise ValueError('invalid rigid world transform')
    rows = [_vector(row, 4, 'rigid world transform') for row in value]
    if any(abs(v-e) > 1e-6 for v, e in zip(rows[3], [0, 0, 0, 1])):
        raise ValueError('invalid rigid world transform')
    for a in range(3):
        for b in range(3):
            if abs(sum(rows[k][a]*rows[k][b] for k in range(3))-(a == b)) > 1e-5:
                raise ValueError('scaled or sheared world transform')
    determinant = sum(rows[0][a]*(rows[1][(a+1)%3]*rows[2][(a+2)%3]
                                 - rows[1][(a+2)%3]*rows[2][(a+1)%3]) for a in range(3))
    if abs(determinant-1) > 1e-5:
        raise ValueError('reflected world transform')
    return rows


def build_captured_emitter(document, binary, clip, world, bindings):
    """Return a new document, BIN bytes and a qualified conversion receipt.

    The snapshot is a single identity mesh node with independent primitives.
    Bindings provide unique primitive indices and their captured world origins.
    Each source slot selects a captured variant in deterministic cyclic order.
    """
    if (document.get('animations') or document.get('skins')
            or len(document.get('meshes', [])) != 1 or len(document.get('nodes', [])) != 1
            or document['nodes'][0].get('mesh') != 0):
        raise ValueError('expected one static captured mesh node')
    node = document['nodes'][0]
    for key, identity in [('translation', [0, 0, 0]), ('rotation', [0, 0, 0, 1]),
                          ('scale', [1, 1, 1]), ('matrix', [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])]:
        if key in node and node[key] != identity:
            raise ValueError('captured mesh node must have identity transform')
    if (len(document.get('buffers', [])) != 1 or 'uri' in document['buffers'][0]
            or type(document['buffers'][0].get('byteLength')) is not int
            or not 0 <= document['buffers'][0]['byteLength'] <= len(binary)):
        raise ValueError('expected one embedded captured buffer')
    matrix = _rigid_world(world)
    primitives = document['meshes'][0]['primitives']
    if not isinstance(bindings, list) or not bindings:
        raise ValueError('missing captured primitive bindings')
    selected, origins = [], []
    for binding in bindings:
        index = binding.get('primitive')
        if type(index) is not int or not 0 <= index < len(primitives) or index in selected:
            raise ValueError('invalid or duplicate captured primitive binding')
        selected.append(index);origins.append(_vector(binding.get('origin'), 3, 'captured origin'))
    if clip.get('format') != 'NativeSplineRigidClip/1':
        raise ValueError('unsupported source clip format')
    period = _number(clip.get('period'), 'clip period')
    slots = clip.get('slots')
    if period <= 0 or not isinstance(slots, list) or not 1 <= len(slots) <= 1024:
        raise ValueError('invalid source clip slots or period')
    out = copy.deepcopy(document);blob = bytearray(binary)
    out['nodes'] = [];out['meshes'] = [dict(name='captured-template-'+str(i), primitives=[copy.deepcopy(primitives[i])]) for i in selected]
    out['scenes'] = [dict(nodes=list(range(len(slots))))];out['scene'] = 0
    extension = 'AZURE_node_opacity'
    used = out.setdefault('extensionsUsed', [])
    if extension not in used:used.append(extension)
    animation = dict(name=clip.get('name', 'captured-flow'), samplers=[], channels=[],
                     extensions={extension: dict(schemaVersion=1, channels=[])})
    out['animations'] = [animation]

    def accessor(rows, width):
        blob.extend(b'\0'*(-len(blob) % 4));offset = len(blob)
        flat = [v for row in rows for v in row]
        blob.extend(struct.pack('<'+'f'*len(flat), *flat))
        view = len(out.setdefault('bufferViews', []))
        out['bufferViews'].append(dict(buffer=0, byteOffset=offset, byteLength=len(flat)*4))
        index = len(out.setdefault('accessors', []))
        item = dict(bufferView=view, componentType=5126, count=len(rows), type='SCALAR' if width == 1 else 'VEC3')
        if width == 1:item.update(min=[min(flat)], max=[max(flat)])
        out['accessors'].append(item);return index

    total = 0
    for index, slot in enumerate(slots):
        times = slot.get('times', []);total += len(times)
        if (not isinstance(times, list) or len(times) < 2 or total > 1000000
                or any(_number(t, 'sample time') < 0 for t in times)
                or times[0] != 0 or abs(times[-1]-period) > max(1e-5, period*1e-6)
                or any(a >= b for a, b in zip(times, times[1:]))):
            raise ValueError('invalid source sample times')
        for field in ['positions', 'scales', 'opacities']:
            if len(slot.get(field, [])) != len(times):raise ValueError('incomplete source '+field)
        positions = [_vector(v, 3, 'source position') for v in slot['positions']]
        scales = [_vector(v, 3, 'source scale') for v in slot['scales']]
        if any(any(v < 0 for v in row) or (any(v == 0 for v in row) and any(v != 0 for v in row)) for row in scales):
            raise ValueError('invalid source lifetime scale')
        opacities = [_number(v, 'source opacity') for v in slot['opacities']]
        if any(not 0 <= v <= 1 for v in opacities):raise ValueError('invalid source opacity')
        if (any(abs(a-b) > 1e-5 for a, b in zip(positions[0], positions[-1]))
                or scales[0] != scales[-1] or abs(opacities[0]-opacities[-1]) > 1e-6):
            raise ValueError('source loop endpoints disagree')
        template = index % len(selected);origin = origins[template]
        translations = [[sum(matrix[row][k]*point[k] for k in range(3))+matrix[row][3]-origin[row]
                         for row in range(3)] for point in positions]
        visibility = [[1, 1, 1] if row[0] > 0 else [0, 0, 0] for row in scales]
        out['nodes'].append(dict(name='captured-slot-'+str(index), mesh=template,
                                 translation=translations[0], scale=[1, 1, 1],
                                 extensions={extension: dict(schemaVersion=1, opacity=opacities[0])}))
        time_accessor = accessor([[t] for t in times], 1)
        for path, rows, interpolation in [('translation', translations, 'LINEAR'), ('scale', visibility, 'STEP')]:
            sampler = len(animation['samplers'])
            animation['samplers'].append(dict(input=time_accessor, output=accessor(rows, 3), interpolation=interpolation))
            animation['channels'].append(dict(sampler=sampler, target=dict(node=index, path=path)))
        sampler = len(animation['samplers'])
        animation['samplers'].append(dict(input=time_accessor, output=accessor([[v] for v in opacities], 1), interpolation='LINEAR'))
        animation['extensions'][extension]['channels'].append(dict(sampler=sampler, node=index))
    blob.extend(b'\0'*(-len(blob) % 4));out['buffers'] = [dict(byteLength=len(blob))]
    report = dict(format='NativeCapturedSplineGlb/1', identity=clip.get('identity'), name=clip.get('name'),
                  period=period, slotCount=len(slots), templatePrimitives=selected, sourceKeys=total,
                  templateTransformPolicy='captured-orientation-and-size', initialPhase='authored-prewarm-zero',
                  nativeRandomVerified=False, nativeGlobalPhaseVerified=False, nativeActivationVerified=False)
    return out, bytes(blob), report
