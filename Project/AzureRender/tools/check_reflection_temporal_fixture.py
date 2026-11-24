"""Check GPU temporal outputs against the frozen native signal formats."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def decode(words, delta=0):
    result=[]
    for offset,bits in ((0,6),(11,6),(22,5)):
        code=((words>>offset)&((1<<(bits+5))-1)).astype(np.int64)+delta
        code=np.clip(code,0,(1<<(bits+5))-1)
        exponent=code>>bits;mantissa=code&((1<<bits)-1)
        value=np.where(exponent>0,np.ldexp(1+mantissa.astype(float)/(1<<bits),exponent-15),np.ldexp(mantissa.astype(float),1-15-bits))
        result.append(np.where(exponent==31,np.where(mantissa==0,np.inf,np.nan),value))
    return np.stack(result,-1)


def check(fixture, actual, output):
    root=Path(__file__).resolve().parents[1]
    referenceRoot=root/'docs/acceptance/nr/evidence/2026-10-28/reflection-pyramid-fixture'
    metadata=json.loads((fixture/'fixture.json').read_text(encoding='utf-8'))
    assert metadata['schemaVersion']==1
    pixels=metadata['width']*metadata['height'];assert 0<pixels<=8192*8192
    for name,info in metadata['files'].items():
        path=fixture/name
        assert path.resolve().is_relative_to(fixture.resolve())
        assert path.stat().st_size==info['bytes']<=32*1024*1024
        assert hashlib.sha256(path.read_bytes()).hexdigest()==info['sha256']
    words=np.fromfile(referenceRoot/'texture-16552-mip-0.bin',dtype='<u4')
    reference=decode(words);above=decode(words,1);below=decode(words,-1)
    maskReference=np.fromfile(referenceRoot/'confidence-mip-0.bin',dtype='u1').astype(float)
    assert len(words)==len(maskReference)==pixels
    results=[]
    for name in ('packed','float'):
        if name=='packed':
            color=decode(np.fromfile(actual/(name+'-0-color.bin'),dtype='<u4'))
            mask=np.fromfile(actual/(name+'-0-confidence.bin'),dtype='u1').astype(float)
        else:
            color=np.fromfile(actual/(name+'-0-color.bin'),dtype='<f4').reshape(-1,4)[:,:3].astype(float)
            mask=np.fromfile(actual/(name+'-0-confidence.bin'),dtype='<f4').astype(float)*255
        assert color.shape==reference.shape and mask.shape==maskReference.shape
        assert np.isfinite(color).all() and np.isfinite(mask).all()
        units=np.abs(color-reference)/np.where(color>=reference,above-reference,reference-below)
        error=np.abs(mask-maskReference)
        results.append(dict(format=name,pixels=pixels,differentColorPixels=int(np.any(color!=reference,axis=1).sum()),
            maximumNativeColorUnits=float(units.max()),colorOverBudget=int((units>1.01).sum()),
            maximumConfidenceByteError=float(error.max()),confidenceOverBudget=int((error>1.01).sum())))
    current=decode(np.fromfile(fixture/'current-color.bin',dtype='<u4')).astype(np.float32)
    baseline=np.fromfile(actual/'float-1-color.bin',dtype='<f4').reshape(-1,4)[:,:3]
    baselineMask=np.fromfile(actual/'float-1-confidence.bin',dtype='<f4')
    sourceMask=np.fromfile(fixture/'current-confidence.bin',dtype='u1').astype(np.float32)/np.float32(255)
    baselineError=float(np.max(np.abs(baseline-np.maximum(current,np.float32(.001)))))
    maskError=float(np.max(np.abs(baselineMask-sourceMask)))
    report=dict(configuration=actual.name,budgets=dict(colorNativeUnits=1.01,confidenceBytes=1.01),results=results,
        zeroWeightBaselineMaximum=baselineError,zeroWeightMaskMaximum=maskError,nativeHistoryChangedPixels=int(np.any(reference!=current,axis=1).sum()))
    output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))
    assert all(x['colorOverBudget']==0 and x['confidenceOverBudget']==0 for x in results)
    assert baselineError<1e-7 and maskError<1e-7


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--fixture',type=Path,required=True)
    parser.add_argument('--actual',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    check(args.fixture,args.actual,args.output)
