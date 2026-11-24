"""Audit source transform bindings against one exact target hierarchy and Avatar.

Target-absent channels retain their original sample indices. Their sample buffers
remain required evidence; projection never substitutes a transform or pose.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zlib


def project_transform_bindings(audit, source, avatar, *, allow_target_absent=False):
    count=audit['output_track_count'];bindings=audit['bindings']
    if not isinstance(count,int) or count<=0 or len(bindings)<count*2:
        raise ValueError('Invalid transform binding blocks')
    paths={};seen=set()
    for row in source['transforms']:
        path=row['path']
        if not isinstance(path,str) or not path or path in seen:
            raise ValueError('Invalid or duplicate target hierarchy path')
        seen.add(path);parts=path.split('/')
        for index in range(len(parts)):
            crc=zlib.crc32('/'.join(parts[index:]).encode())
            paths.setdefault(crc,set()).add(path)
    avatar_hashes={int(k) for k in avatar['m_TOS']}
    avatar_hashes.update(avatar['m_Avatar']['m_SkeletonNameIDArray'])
    bound=[];unbound=[];bound_paths=set()
    for index in range(count):
        p,q=bindings[index],bindings[index+count]
        if p['type']!='Transform' or q['type']!='Transform' or p['attribute']!=1 or q['attribute']!=2 or p['path']!=q['path']:
            raise ValueError('Transform binding blocks do not align')
        crc=p['path']
        if not isinstance(crc,int) or not 0<crc<=0xffffffff:
            raise ValueError('Invalid transform path CRC')
        candidates=paths.get(crc,set())
        if len(candidates)>1:raise ValueError('Target transform CRC is ambiguous')
        if candidates:
            path=next(iter(candidates))
            if path in bound_paths:raise ValueError('Multiple tracks bind one target transform')
            bound_paths.add(path)
            bound.append(dict(track=index,pathHash=crc,sourceTransformPath=path))
        else:
            if crc in avatar_hashes:
                raise ValueError('Target Avatar references a transform missing from its source hierarchy')
            if not allow_target_absent:raise ValueError('Transform path is absent from target')
            unbound.append(dict(track=index,pathHash=crc,positionBinding=index,rotationBinding=index+count,
                                reason='absent_from_target_hierarchy_and_avatar'))
    if not bound:raise ValueError('Projection requires bound target transforms')
    return dict(format='AzureTargetBindingProjection/1',transformTrackCount=count,
                boundTracks=bound,unboundTracks=unbound,
                policy='target_absent_preserved' if allow_target_absent else 'strict',
                nativeRuntimeEquivalenceVerified=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clip-audit', type=Path, required=True)
    parser.add_argument('--source-audit', type=Path, required=True)
    parser.add_argument('--avatar', type=Path, required=True)
    parser.add_argument('--allow-target-absent', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    inputs = [args.clip_audit, args.source_audit, args.avatar]
    result = project_transform_bindings(
        *(json.loads(p.read_text(encoding='utf-8-sig')) for p in inputs),
        allow_target_absent=args.allow_target_absent)
    result['inputs'] = [dict(path=str(p.resolve()), sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                        for p in inputs]
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2, allow_nan=False)
    print(json.dumps(dict(bound=len(result['boundTracks']), unbound=len(result['unboundTracks']),
                          policy=result['policy'])))


if __name__ == '__main__':
    main()
