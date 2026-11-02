"""Append a loop-closed TRS clip while preserving original GLB data."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import struct

from convert_material_semantics import read_asset, write_asset


def append_loop_clip(document, binary, clip_index, name, blend_seconds=.15):
    if (type(clip_index) is not int or not 0 <= clip_index < len(document.get('animations', []))
            or not isinstance(name, str) or not name
            or any(a.get('name') == name for a in document['animations'])):
        raise ValueError('Invalid source clip or duplicate output name')
    if type(blend_seconds) not in (int, float) or not math.isfinite(blend_seconds) or blend_seconds <= 0:
        raise ValueError('Invalid loop correction window')
    if (len(document.get('buffers', [])) != 1 or 'uri' in document['buffers'][0]
            or document['buffers'][0].get('byteLength', len(binary)+1) > len(binary)):
        raise ValueError('Expected one embedded animation buffer')
    out = copy.deepcopy(document);blob = bytearray(binary);clip = copy.deepcopy(document['animations'][clip_index])
    paths = {}
    for channel in clip['channels']:
        path = channel['target']['path'];index = channel['sampler']
        if path not in ['translation', 'rotation', 'scale']:
            raise ValueError('Unsupported loop channel '+str(path))
        if index in paths and paths[index] != path:raise ValueError('Shared sampler has conflicting transform paths')
        paths[index] = path

    def read(index, expected):
        a = document['accessors'][index]
        if a.get('componentType') != 5126 or a.get('type') != expected or 'sparse' in a or a.get('normalized'):
            raise ValueError('Unsupported animation accessor')
        v = document['bufferViews'][a['bufferView']];width = {'SCALAR': 1, 'VEC3': 3, 'VEC4': 4}[expected]
        count = a['count'];stride = v.get('byteStride', width*4);offset = v.get('byteOffset', 0)+a.get('byteOffset', 0)
        if (type(count) is not int or not 1 <= count <= 1000000 or v.get('buffer') != 0
                or stride < width*4 or offset < 0 or offset+(count-1)*stride+width*4 > len(binary)
                or a.get('byteOffset', 0)+(count-1)*stride+width*4 > v['byteLength']):
            raise ValueError('Invalid animation buffer range')
        rows = [list(struct.unpack_from('<'+'f'*width, binary, offset+i*stride)) for i in range(count)]
        if any(not math.isfinite(x) for row in rows for x in row):raise ValueError('Non-finite animation sample')
        return rows

    def append(rows, shape):
        flat = [v for row in rows for v in row];blob.extend(b'\0'*(-len(blob) % 4));offset = len(blob)
        blob.extend(struct.pack('<'+'f'*len(flat), *flat));view = len(out['bufferViews'])
        out['bufferViews'].append(dict(buffer=0, byteOffset=offset, byteLength=len(flat)*4))
        index = len(out['accessors']);item = dict(bufferView=view, componentType=5126, count=len(rows), type=shape)
        if shape == 'SCALAR':item.update(min=[min(flat)], max=[max(flat)])
        out['accessors'].append(item);return index

    maximum_duration = 0.;minimum_start = math.inf;changed_tracks = 0;tracks = []
    for index, path in paths.items():
        sampler = clip['samplers'][index]
        interpolation = sampler.get('interpolation', 'LINEAR')
        if interpolation not in ['LINEAR', 'STEP']:raise ValueError('Unsupported loop interpolation')
        times = [row[0] for row in read(sampler['input'], 'SCALAR')]
        values = read(sampler['output'], 'VEC4' if path == 'rotation' else 'VEC3')
        if len(times) != len(values) or times[0] < 0 or any(a >= b for a, b in zip(times, times[1:])):
            raise ValueError('Invalid loop sample times')
        minimum_start = min(minimum_start, times[0]);maximum_duration = max(maximum_duration, times[-1]);tracks.append((sampler,path,interpolation,times,values))
    if not tracks:raise ValueError('Missing loop transform tracks')
    duration = maximum_duration-minimum_start;window = min(blend_seconds, duration)
    boundary = struct.unpack('<f',struct.pack('<f',maximum_duration-window))[0]
    for sampler, path, interpolation, times, values in tracks:
        first = values[0];result = copy.deepcopy(values)
        if path == 'rotation' and any(sum(v*v for v in q) < 1e-12 for q in values):raise ValueError('Invalid loop quaternion')
        if interpolation == 'STEP':
            if values[0] != values[-1]:
                if times[-1] < maximum_duration:
                    times = times+[maximum_duration];result.append(list(first));sampler['input']=append([[t] for t in times],'SCALAR')
                else:result[-1]=list(first)
                changed_tracks += 1
            sampler['output'] = append(result, 'VEC4' if path == 'rotation' else 'VEC3')
            continue
        original_times = list(times)
        def sample(time):
            if time <= times[0]:return list(values[0])
            if time >= times[-1]:return list(values[-1])
            for i in range(len(times)-1):
                if times[i] <= time <= times[i+1]:
                    weight = (time-times[i])/(times[i+1]-times[i]);a=list(values[i]);b=list(values[i+1])
                    if path != 'rotation':return [(1-weight)*x+weight*y for x,y in zip(a,b)]
                    an=math.sqrt(sum(x*x for x in a));bn=math.sqrt(sum(x*x for x in b));a=[x/an for x in a];b=[x/bn for x in b]
                    dot=sum(x*y for x,y in zip(a,b))
                    if dot < 0:b=[-x for x in b];dot=-dot
                    if dot > .9995:
                        q=[(1-weight)*x+weight*y for x,y in zip(a,b)];length=math.sqrt(sum(x*x for x in q));return [x/length for x in q]
                    angle=math.acos(max(-1.,min(1.,dot)));denominator=math.sin(angle)
                    return [(math.sin((1-weight)*angle)*x+math.sin(weight*angle)*y)/denominator for x,y in zip(a,b)]
            raise ValueError('Uncovered loop sample time')
        if duration and values[0] != values[-1]:
            new_times=sorted(set(times+[minimum_start,boundary,maximum_duration]));result=[sample(t) for t in new_times]
            for i, time in enumerate(new_times):
                denominator = maximum_duration-boundary
                t = max(0., min(1., (time-boundary)/denominator)) if denominator>0 else float(time>=maximum_duration)
                weight = t*t*t*(10+t*(-15+6*t))
                if weight <= 0:continue
                target = first
                if path == 'rotation' and sum(a*b for a, b in zip(result[i], first)) < 0:target = [-v for v in first]
                value = [(1-weight)*a+weight*b for a, b in zip(result[i], target)]
                if path == 'rotation':
                    length = math.sqrt(sum(v*v for v in value));value = [v/length for v in value]
                result[i] = value
            result[-1] = list(first)
            if new_times != original_times:sampler['input'] = append([[t] for t in new_times], 'SCALAR')
        if result != values:changed_tracks += 1
        sampler['output'] = append(result, 'VEC4' if path == 'rotation' else 'VEC3')
    clip['name'] = name;out['animations'].append(clip);blob.extend(b'\0'*(-len(blob) % 4));out['buffers'] = [dict(byteLength=len(blob))]
    receipt = dict(format='GltfLoopClosure/1', sourceClip=clip_index, clipIndex=len(out['animations'])-1,
                   name=name, startTime=minimum_start, endTime=maximum_duration, duration=duration, correctionSeconds=blend_seconds, changedTracks=changed_tracks,
                   interpolation='quintic-tail-pose-blend', sourceClipsPreserved=True)
    return out, bytes(blob), receipt


def main():
    parser = argparse.ArgumentParser(__doc__);parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True);parser.add_argument('--clip', type=int, required=True)
    parser.add_argument('--name', required=True);parser.add_argument('--blend-seconds', type=float, default=.15)
    args = parser.parse_args()
    if args.output.exists() or args.input.resolve() == args.output.resolve():raise ValueError('Output must be a new asset')
    document, tail = read_asset(args.input)
    if tail is None or len(tail) < 8 or tail[4:8] != b'BIN\0' or struct.unpack_from('<I', tail)[0] != len(tail)-8:
        raise ValueError('Expected GLB with one BIN chunk')
    model, binary, receipt = append_loop_clip(document, tail[8:], args.clip, args.name, args.blend_seconds)
    args.output.parent.mkdir(parents=True, exist_ok=True);write_asset(args.output, model, struct.pack('<I4s', len(binary), b'BIN\0')+binary)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest();receipt.update(source=str(args.input.resolve()),sourceSha256=sha(args.input),outputSha256=sha(args.output),toolSha256=sha(Path(__file__)))
    args.output.with_suffix(args.output.suffix+'.loop.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt))


if __name__ == '__main__':main()
