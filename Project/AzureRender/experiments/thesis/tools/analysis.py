"""Run-level descriptive statistics and explicit full-frame attachment model."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics

PATHS=('traditional','subpasses','drlr')

def describe(samples):
    if len(samples)<2 or any(type(x) not in (int,float) or not math.isfinite(x) or x<0 for x in samples):
        raise ValueError('at least two finite nonnegative numeric samples required')
    ordered=sorted(samples)
    def quantile(p):
        index=(len(ordered)-1)*p; lower=math.floor(index); upper=math.ceil(index)
        return ordered[lower]+(ordered[upper]-ordered[lower])*(index-lower)
    return dict(n=len(samples),meanMs=statistics.mean(samples),varianceMs2=statistics.variance(samples),
                medianMs=statistics.median(samples),p95Ms=quantile(.95),p99Ms=quantile(.99),
                minMs=ordered[0],maxMs=ordered[-1])


def traffic(width,height,path):
    if type(width) is not int or type(height) is not int or width<=0 or height<=0 or path not in PATHS:
        raise ValueError('positive integer extent and supported path required')
    pixels=width*height
    # Full-frame equivalent accesses: G writes 4+8+4; toon reads 4+8/writes 4;
    # contour reads normal 5*8 and toon 4, writes output 4. Readback excluded.
    # Reuse scenario avoids albedo store+read and the toon normal read (4+4+8).
    return dict(logicalBytes=80*pixels,localReuseScenarioBytes=(80 if path=='traditional' else 64)*pixels,
                normalStoreBytes=8*pixels,contourNormalReadBytes=40*pixels,
                hardwareMeasured=False,unit='uncompressed full-frame-equivalent attachment bytes/frame',
                assumptions=['single sample','depth write counted equally in all paths',
                             'five normal/depth contour reads, one toon read',
                             'local scenario retains normal store for later contour',
                             'no compression, cache, overdraw, vertex or readback model'])


def summarize(run):
    path=run.get('path')
    if path not in PATHS: raise ValueError('unknown execution path')
    gpu=describe(run['gpuMs']); cpu=describe(run['cpuRecordingMs'])
    if gpu['n']!=cpu['n']: raise ValueError('CPU/GPU sample counts differ')
    warmup=run['warmup']
    if type(warmup) is not int or warmup<0: raise ValueError('invalid warmup')
    total=gpu['n']+warmup
    expected=dict(geometryDraws=total,toonDraws=total,contourDraws=total,
                  nextSubpassCalls=total if path=='subpasses' else 0,
                  beginRenderingCalls=total if path=='drlr' else 0)
    if any(run.get(k)!=v for k,v in expected.items()): raise ValueError('execution counters do not match claimed path')
    if run.get('validationErrors',0): raise ValueError('validation errors invalidate the run')
    return dict(path=path,gpu=gpu,cpuRecording=cpu,
                traffic=traffic(run['width'],run['height'],path),
                inference='descriptive; independent repetitions and formal qualification required')


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('runs',nargs='+',type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    results=[]; seen=set()
    for path in args.runs:
        data=path.read_bytes(); digest=hashlib.sha256(data).hexdigest()
        if digest in seen: raise ValueError('duplicate raw run cannot count as independent repetition')
        seen.add(digest)
        run=json.loads(data)
        results.append(dict(source=str(path.absolute()),sha256=digest,device=run['device'],
                            complexity=run['complexity'],extent=[run['width'],run['height']],
                            declaredStatus=run['status'],**summarize(run)))
    args.output.write_text(json.dumps(dict(schemaVersion=1,runs=results),indent=2)+'\n',encoding='utf-8')
    print('Summarized',len(results),'run records; no hypothesis test or pooled-frame inference')


if __name__=='__main__': main()
