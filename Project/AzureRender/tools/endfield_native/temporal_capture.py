"""Measured frame-10401 TAAU inputs; no engine runtime resource identities."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

def decode_motion(packed: np.ndarray) -> np.ndarray:
    """E1579 t2.xy -> current-minus-previous UV, upper-left origin."""
    packed=np.asarray(packed)
    if packed.dtype!=np.dtype('uint32'):
        raise ValueError('Motion requires packed uint32 R10G10B10A2 words')
    fields=np.stack((packed&1023,(packed>>10)&1023),axis=-1)
    encoded=fields.astype(np.float32)/np.float32(1023)
    magnitude=encoded*np.float32(2)-np.float32(1)
    magnitude=magnitude*magnitude
    magnitude=magnitude*magnitude
    return np.sign(encoded-np.float32(.5))*magnitude

def history_coordinates(position,extent,packed):
    position=np.asarray(position,dtype=np.float32)
    extent=np.asarray(extent,dtype=np.float32)
    motion=decode_motion(packed)
    if extent.shape!=(2,) or not np.isfinite(extent).all() or np.any(extent<=0):
        raise ValueError('History extent requires two finite positive dimensions')
    if position.shape!=motion.shape or not np.isfinite(position).all():
        raise ValueError('Pixel positions must match motion and be finite')
    return position/extent-motion

def summarize_planes(mask,history,resolved):
    mask=np.asarray(mask);history=np.asarray(history);resolved=np.asarray(resolved)
    if mask.dtype!=np.uint8 or mask.ndim!=2 or history.ndim!=3 or history.shape[-1]!=4 or resolved.shape!=history.shape:
        raise ValueError('Expected R8 mask and matching RGBA history/output')
    if history.shape[:2]!=(mask.shape[0]*4,mask.shape[1]*4):
        raise ValueError('Captured mask must have quarter-size dimensions')
    if not np.isfinite(history).all() or not np.isfinite(resolved).all():
        raise ValueError('Captured HDR planes must be finite')
    def stats(a):return dict(minimum=float(a.min()),maximum=float(a.max()),nonzeroPixels=int(np.count_nonzero(a)),pixels=int(a.size))
    return dict(mask=stats(mask),historyAlpha=stats(history[:,:,3]),resolvedAlpha=stats(resolved[:,:,3]),
        nonzeroAlphaObserved=bool(np.any(history[:,:,3]>0) or np.any(resolved[:,:,3]>0)))

def analyze(run:Path,constants:Path,shader:Path):
    expected_shader='b84733f669ffdc63699e603c7e276a8992ed3a4220da4aae2a2936dc8eab0509'
    digest=hashlib.sha256(shader.read_bytes()).hexdigest()
    if digest!=expected_shader:raise ValueError('TAAU shader fingerprint requires a fresh source audit')
    inputs=[];planes={}
    specifications=[('t0-160192','R11G11B10_FLOAT','<u4',1),('t1-141967','D32S8_TYPELESS','<u4',2),
        ('t2-141846','R10G10B10A2_UNORM','<u4',1),('t3-141914','R8_UNORM','u1',1),
        ('t4-16349','R16G16B16A16_FLOAT','<f2',4),('resolved','R16G16B16A16_FLOAT','<f2',4)]
    for name,fmt,dtype,components in specifications:
        receipt=json.loads((run/(name+'-receipt.json')).read_text(encoding='utf-8-sig'))
        d=receipt['data'];raw=(run/(name+'.bin')).read_bytes()
        if not receipt.get('ok') or not d.get('complete') or d['format']!=fmt or d['depth']!=1:
            raise ValueError('Incomplete or incompatible input: '+name)
        if hashlib.sha256(raw).hexdigest()!=d['sha256'] or len(raw)!=d['bytes']:
            raise ValueError('Captured bytes differ from receipt: '+name)
        shape=(d['height'],d['width'])+((components,) if components>1 else ())
        planes[name]=np.frombuffer(raw,dtype=dtype).reshape(shape)
        inputs.append({k:d[k] for k in ('eid','rid','width','height','format','bytes','sha256')})
    cb=json.loads(constants.read_text(encoding='utf-8-sig'))['data']['cbufs']
    values={b['slot']:{v['name']:v['value'] for v in b['variables']} for b in cb}
    motion=decode_motion(planes['t2-141846'])
    return dict(schemaVersion=1,event=1579,shaderSha256=digest,inputs=inputs,
        jitterPixels=values[0]['cb0_v19'][:2],jitterUv=values[0]['cb0_v19'][2:],
        inputExtent=values[1]['cb1_v6'][:2],outputExtent=values[1]['cb1_v7'][:2],
        motion=dict(direction='current-minus-previous',units='normalized UV',origin='upper-left',
            minimumUv=motion.min(axis=(0,1)).tolist(),maximumUv=motion.max(axis=(0,1)).tolist()),
        measurements=summarize_planes(planes['t3-141914'],planes['t4-16349'],planes['resolved']),
        scope='Single captured frame: input/decode evidence; temporal convergence and moving quality require sequences')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',type=Path,required=True);parser.add_argument('--constants',type=Path,required=True)
    parser.add_argument('--shader',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();report=analyze(args.run,args.constants,args.shader)
    args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status='passed',event=report['event'],measurements=report['measurements'])))

if __name__=='__main__':main()
