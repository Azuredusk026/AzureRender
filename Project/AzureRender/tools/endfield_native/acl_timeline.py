"""Plan aligned decoder queries including ACL's implicit wrapping endpoint.

The plan describes decoder seconds. Mapping the original Unity clip interval
onto that domain requires separate game-consumer evidence.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


def build_sample_plan(buffers, *, source_start, source_stop, maximum_frames=36001):
    if set(buffers) != {'transform', 'root_motion', 'float'}:
        raise ValueError('timeline streams must contain transform, root_motion and float')
    if any(isinstance(v, bool) or not isinstance(v, (int,float)) or not math.isfinite(v)
           for v in (source_start, source_stop)) or source_start < 0 or source_stop <= source_start:
        raise ValueError('source clip domain must be finite, nonnegative and increasing')
    if isinstance(maximum_frames, bool) or not isinstance(maximum_frames, int) or not 2 <= maximum_frames <= 1000000:
        raise ValueError('timeline frame budget must be an integer in [2,1000000]')
    streams = {}
    for name, data in buffers.items():
        if len(data) < 32:
            raise ValueError(name+': truncated ACL header')
        size, _, magic, version, algorithm, kind, tracks, count, rate, misc = struct.unpack_from('<IIIHBBIIfI', data)
        if size < 32 or size > len(data) or magic != 0xac11ac11 or not 7 <= version <= 10 or algorithm != 0:
            raise ValueError(name+': unsupported or malformed ACL header')
        if kind != (12 if name == 'transform' else 0) or tracks == 0 or count == 0:
            raise ValueError(name+': invalid track type or dimensions')
        if not math.isfinite(rate) or rate <= 0:
            raise ValueError(name+': invalid sample rate')
        wrapped = version > 7 and bool(misc & (1 << 30))
        intervals = count if wrapped else count-1
        if intervals == 0 or intervals+1 > maximum_frames:
            raise ValueError(name+': nonpositive duration or frame budget exceeded')
        streams[name] = dict(stored_frames=count, evaluated_frames=intervals+1,
            sample_rate=rate, wrap_optimized=wrapped, decoder_duration=intervals/rate,
            sha256=hashlib.sha256(data).hexdigest(), version=version)
    rates = [s['sample_rate'] for s in streams.values()]
    durations = [s['decoder_duration'] for s in streams.values()]
    if max(rates)-min(rates) > 1e-6:
        raise ValueError('ACL streams have different sample rates')
    if max(durations)-min(durations) > 1e-6 or len({s['evaluated_frames'] for s in streams.values()}) != 1:
        raise ValueError('ACL streams have different decoder durations')
    duration = durations[0]
    if not math.isfinite(duration):
        raise ValueError('ACL decoder duration must be finite')
    count = next(iter(streams.values()))['evaluated_frames']
    return dict(format='EndfieldAclTimeline/1', source_clip_domain=[source_start, source_stop],
        decoder_duration=duration, sample_times=[i/rates[0] for i in range(count)], streams=streams,
        game_time_mapping_verified=False,
        sampling_policy='Query each original decoder at every planned time, including its endpoint; preserve stored samples separately')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory',type=Path,required=True)
    parser.add_argument('--source-start',type=float,required=True)
    parser.add_argument('--source-stop',type=float,required=True)
    parser.add_argument('--maximum-frames',type=int,default=36001)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    buffers={name:(args.directory/(name+'.acl')).read_bytes() for name in ('transform','root_motion','float')}
    plan=build_sample_plan(buffers,source_start=args.source_start,source_stop=args.source_stop,
        maximum_frames=args.maximum_frames)
    args.output.write_text(json.dumps(plan,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(format=plan['format'],frames=len(plan['sample_times']),duration=plan['decoder_duration'])))


if __name__=='__main__':
    main()
