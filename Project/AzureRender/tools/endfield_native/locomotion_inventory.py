"""Validate source-bound native locomotion samples without asserting runtime fidelity."""
import argparse
import copy
import hashlib
import json
import math
import struct
from pathlib import Path

try:
    from .acl_timeline import build_sample_plan
except ImportError:
    from acl_timeline import build_sample_plan


def static_pose_override(document, node_names):
    """Use an explicit named node set's stored rest pose; preserve other animation."""
    if len(set(node_names)) != len(node_names):raise ValueError('Duplicate override node')
    selected=set()
    for name in node_names:
        matches=[i for i,n in enumerate(document['nodes']) if n.get('name')==name]
        if len(matches)!=1:raise ValueError('Missing or ambiguous override node: '+name)
        selected.add(matches[0])
    result=copy.deepcopy(document);removed=0
    for action in result.get('animations',[]):
        kept=[]
        for channel in action['channels']:
            if channel['target']['node'] in selected and channel['target']['path'] in ('translation','rotation','scale'):
                removed+=1
            else:kept.append(channel)
        action['channels']=kept
    return result,{'policy':'explicit_static_local_pose_override','node_names':node_names,
                   'removed_channels':removed,'native_neutral_semantics_verified':False,
                   'source_animation_preservation':'original unmodified artifact remains required'}


def append_animation(base, base_tail, source, source_tail, clip_name):
    """Append one source action by unique hierarchy paths; keep base materials and idle."""
    def paths(doc):
        parents = {}
        for i,node in enumerate(doc['nodes']):
            for child in node.get('children',[]):
                if child in parents: raise ValueError('Multiple node parents')
                parents[child] = i
        def path(i, visiting):
            if i in visiting: raise ValueError('Cyclic node hierarchy')
            name=doc['nodes'][i].get('name')
            if not name: raise ValueError('Unnamed animation node')
            return (path(parents[i],visiting|{i}) if i in parents else ())+(name,)
        result=[path(i,set()) for i in range(len(doc['nodes']))]
        if len(set(result))!=len(result):raise ValueError('Ambiguous node identity')
        return result
    def binary(tail):
        if len(tail)<8:raise ValueError('Missing GLB BIN chunk')
        size,kind=struct.unpack_from('<I4s',tail)
        if kind!=b'BIN\0' or size+8!=len(tail):raise ValueError('Unsupported GLB chunk layout')
        return tail[8:]
    bp=paths(base);sp=paths(source);lookup={p:i for i,p in enumerate(bp)}
    selected=[a for a in source.get('animations',[]) if a.get('name')==clip_name]
    if len(selected)!=1 or any(a.get('name')==clip_name for a in base.get('animations',[])):
        raise ValueError('Ambiguous or duplicate animation name')
    result=copy.deepcopy(base);action=copy.deepcopy(selected[0]);blob=bytearray(binary(base_tail));source_blob=binary(source_tail)
    for channel in action['channels']:
        src=channel['target']['node']; target=lookup.get(sp[src])
        if target is None:raise ValueError('Animation skeleton identity mismatch')
        for k,default in [('translation',[0,0,0]),('rotation',[0,0,0,1]),('scale',[1,1,1])]:
            a=source['nodes'][src].get(k,default);b=base['nodes'][target].get(k,default)
            if len(a)!=len(b) or any(not math.isfinite(x) or not math.isfinite(y) or abs(x-y)>1e-5 for x,y in zip(a,b)):
                raise ValueError('Animation rest transform mismatch')
        if 'matrix' in source['nodes'][src] or 'matrix' in base['nodes'][target]:raise ValueError('Matrix rest nodes require explicit conversion')
        channel['target']['node']=target
    mapped={}
    def accessor(index):
        if index in mapped:return mapped[index]
        a=copy.deepcopy(source['accessors'][index])
        if 'sparse' in a:raise ValueError('Sparse animation accessor unsupported')
        v=copy.deepcopy(source['bufferViews'][a['bufferView']])
        if v.get('buffer',0)!=0:raise ValueError('External animation buffer unsupported')
        start=v.get('byteOffset',0);end=start+v['byteLength']
        if start<0 or end>len(source_blob):raise ValueError('Animation buffer range invalid')
        blob.extend(b'\0'*(-len(blob)%4));v['buffer']=0;v['byteOffset']=len(blob);blob.extend(source_blob[start:end])
        a['bufferView']=len(result.setdefault('bufferViews',[]));result['bufferViews'].append(v)
        dest=len(result.setdefault('accessors',[]));result['accessors'].append(a);mapped[index]=dest
        return dest
    for sampler in action['samplers']:
        sampler['input']=accessor(sampler['input']);sampler['output']=accessor(sampler['output'])
    result.setdefault('animations',[]).append(action)
    blob.extend(b'\0'*(-len(blob)%4));result['buffers']=[{'byteLength':len(blob)}]
    return result,struct.pack('<I4s',len(blob),b'BIN\0')+bytes(blob)


def analyze_clip(clip, hierarchy, frames, expected_paths, loop_source=None, decoder_timeline=None):
    if clip['version'] != 10:
        raise ValueError('Unsupported clip version')
    paths = [row['path'] for row in hierarchy]
    if len(set(paths)) != len(paths) or set(paths) != set(expected_paths):
        raise ValueError('Source skeleton paths differ')
    if not hierarchy or hierarchy[0]['parent_index'] != -1:
        raise ValueError('Invalid skeleton root')
    for i, row in enumerate(hierarchy):
        if row['index'] != i or (i and not 0 <= row['parent_index'] < i):
            raise ValueError('Invalid skeleton ordering')
    times = [row['time'] for row in frames]
    if len(times) < 2 or not all(math.isfinite(t) for t in times) or any(b <= a for a,b in zip(times,times[1:])):
        raise ValueError('Invalid sample time domain')
    if not all(math.isfinite(clip[k]) for k in ('start', 'stop')) or clip['stop'] <= clip['start']:
        raise ValueError('Invalid clip time domain')
    expected_start,expected_stop=clip['start'],clip['stop']
    domain_kind='source_clip_seconds'
    if decoder_timeline is not None:
        plan=decoder_timeline
        if not isinstance(plan,dict) or plan.get('format')!='EndfieldAclTimeline/1' or plan.get('game_time_mapping_verified') is not False:
            raise ValueError('Invalid decoder timeline domain provenance')
        def finite_number(value):
            return type(value) in (int,float) and math.isfinite(value)
        source=plan.get('source_clip_domain',[])
        if not isinstance(source,list) or len(source)!=2 or any(not finite_number(a) or abs(a-b)>1e-5
                for a,b in zip(source,[clip['start'],clip['stop']])):
            raise ValueError('Decoder timeline source domain mismatch')
        planned=plan.get('sample_times',[]);duration=plan.get('decoder_duration',math.nan)
        if not isinstance(planned,list) or len(planned)!=len(times) or not finite_number(duration) or duration<=0 or not planned:
            raise ValueError('Decoder sample domain dimensions mismatch')
        if any(not finite_number(a) or abs(a-b)>1e-5 for a,b in zip(planned,times)):
            raise ValueError('Decoder sample domain does not match evaluated frames')
        expected_start,expected_stop=0,duration;domain_kind='decoder_seconds'
    if abs(times[0]-expected_start) > 1e-5 or abs(times[-1]-expected_stop) > 1e-5:
        raise ValueError('Incomplete sample domain')
    for frame in frames:
        if len(frame['locals']) != len(hierarchy):
            raise ValueError('Missing skeleton pose')
        for pose in frame['locals']:
            for field, count in [('position',3), ('rotation',4), ('scale',3)]:
                if len(pose[field]) != count or not all(math.isfinite(v) for v in pose[field]):
                    raise ValueError('Pose must have finite components')
            if abs(sum(v*v for v in pose['rotation'])-1) > .0021:
                raise ValueError('Nonunit rotation')
    roots = [frame['locals'][0]['position'] for frame in frames]
    speeds = [math.dist(a,b)/(tb-ta) for a,b,ta,tb in zip(roots,roots[1:],times,times[1:])]
    distance = sum(math.dist(a,b) for a,b in zip(roots,roots[1:]))
    duration = times[-1]-times[0]
    max_translation = max((math.dist(a['position'], b['position']) for a,b in zip(frames[0]['locals'][1:], frames[-1]['locals'][1:])), default=0)
    max_rotation = max((2*math.acos(min(1,abs(sum(x*y for x,y in zip(a['rotation'],b['rotation']))))) for a,b in zip(frames[0]['locals'][1:],frames[-1]['locals'][1:])), default=0)
    skeleton = [{'path': x['path'], 'parent_index': x['parent_index'], 'rest_local': x['rest_local']} for x in hierarchy]
    if loop_source is not None:
        if (loop_source['cab'], loop_source['pathId']) != (clip['source'], clip['path_id']):
            raise ValueError('Loop source identity mismatch')
        if type(loop_source['m_LoopTime']) is not bool or any(abs(loop_source[k]-clip[k]) > 1e-5 for k in ('start','stop')):
            raise ValueError('Loop source domain mismatch')
    return {'format': 'NativeLocomotionInventory/1',
            'source': {k: clip[k] for k in ('source','path_id','version','m_name')},
            'skeleton_sha256': hashlib.sha256(json.dumps(skeleton,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
            'skeleton_nodes': len(hierarchy), 'frames': len(frames), 'duration_seconds': duration,
            'time_domain': {'kind': domain_kind, 'source_clip_interval_seconds': [clip['start'],clip['stop']],
                            'evaluated_interval_seconds': [times[0],times[-1]], 'game_time_mapping_verified': False},
            'coordinates': 'Unity source local hierarchy; meters; Y up',
            'root': {'kind': 'moving_root' if distance > 1e-4 else 'in_place',
                     'displacement_m': [b-a for a,b in zip(roots[0],roots[-1])],
                     'path_length_m': distance, 'mean_speed_mps': distance/duration,
                     'samples': [{'time': t,'position': p} for t,p in zip(times,roots)],
                     'speed_samples': [{'time': t,'speed_mps': v} for t,v in zip(times[1:],speeds)]},
            'loop': {'authored_loop_verified': loop_source is not None,
                     'authored_loop_enabled': loop_source['m_LoopTime'] if loop_source else None,
                     'source': loop_source, 'candidate_interval_seconds': [times[0],times[-1]],
                     'nonroot_endpoint_max_translation_m': max_translation,
                     'nonroot_endpoint_max_rotation_radians': max_rotation,
                     'seam_status': 'measured_not_accepted'},
            'foot_contacts': {'source': 'unavailable', 'intervals': [], 'status': 'requires_source_events_or_validated_world_foot_analysis'},
            'runtime_equivalence_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--decoded', type=Path, required=True)
    parser.add_argument('--source-audit', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--loop-source', type=Path)
    parser.add_argument('--decoder-timeline', type=Path)
    args = parser.parse_args()
    files = [args.decoded/'native/clip_audit.json', args.decoded/'humanoid/merged/source_hierarchy.json',
             args.decoded/'humanoid/merged/source_local_frames.json', args.source_audit]
    clip,hierarchy,frames,audit = [json.loads(p.read_text(encoding='utf-8-sig')) for p in files]
    loop_source = json.loads(args.loop_source.read_text(encoding='utf-8-sig')) if args.loop_source else None
    timeline=json.loads(args.decoder_timeline.read_text(encoding='utf-8-sig')) if args.decoder_timeline else None
    if timeline is not None:
        buffers={}
        for name in ('transform','root_motion','float'):
            raw=args.decoded/'native'/(name+'.acl')
            buffers[name]=raw.read_bytes()
            files.append(raw)
        regenerated=build_sample_plan(buffers,source_start=clip['start'],source_stop=clip['stop'])
        if timeline!=regenerated:
            raise ValueError('Decoder timeline does not match source buffers and clip domain')
        files.append(args.decoder_timeline)
    result = analyze_clip(clip,hierarchy,frames,[x['path'] for x in audit['transforms']],loop_source,timeline)
    if args.loop_source: files.append(args.loop_source)
    result['inputs'] = [{'path': str(p.resolve()), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]
    with args.output.open('x', encoding='utf8') as f:
        json.dump(result,f,indent=2,allow_nan=False)
    print(json.dumps({k:result[k] for k in ('format','frames','duration_seconds','skeleton_sha256')}))


if __name__ == '__main__': main()
