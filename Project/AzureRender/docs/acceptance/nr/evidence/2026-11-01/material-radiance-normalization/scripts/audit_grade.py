import hashlib,json,struct
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;project=run/'project';captures=run/'native-captures'
source=captures/'near-source-isolated/frames/frame_000000.png';world=captures/'near-world-normalized/frames/frame_000000.png'
path=project/'native-gi/captured-3534.azlut';raw=path.read_bytes();assert raw[:8]==b'AZLUT001'
length=struct.unpack_from('<I',raw,8)[0];header=json.loads(raw[12:12+length]);n=header['dimension']
assert header['inputDomain']=='logc3-full-log' and header['outputDomain']=='linear'
table=np.frombuffer(raw,dtype='<f4',offset=12+length).reshape(n,n,n,4)
def linear(a):
    a=np.asarray(a,dtype=float)/255
    return np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)
def grade(value):
    coord=np.clip(np.log2(np.maximum(value*5.555555820465088+.047995999455451965,1e-30))*.3010300099849701*.244160994887352+.3860360085964203,0,1)*(n-1)
    low=np.floor(coord).astype(int);high=np.minimum(low+1,n-1);f=coord-low;result=np.zeros_like(value)
    for corner in range(8):
        cells=np.where(np.array([(corner>>i)&1 for i in range(3)]),high,low)
        weight=np.prod(np.where(np.array([(corner>>i)&1 for i in range(3)]),f,1-f),axis=-1)
        result+=table[cells[...,1],cells[...,2],cells[...,0],:3]*weight[...,None]
    return result
a=np.array(Image.open(source).convert('RGB'));b=np.array(Image.open(world).convert('RGB'))
regions=json.loads((root/'build/runs/20261101-014745-joint-display-domain-diagnosis-8779a0/component-comparison.json').read_text())['pairs'][0]['regions'][:5]
rows=[]
for region in regions:
    x,y,z,w=region['bounds'];rgb=a[y:w,x:z];actual=linear(b[y:w,x:z]);prediction=grade(linear(rgb));valid=np.all((rgb>0)&(rgb<255),axis=-1)
    lum=lambda v:v@np.array([.2126,.7152,.0722])
    # LDR inversion is a diagnostic estimate. Clipped pixels are excluded.
    error=np.abs(actual-prediction)[valid];wanted=float(np.median(lum(prediction)[valid]));observed=float(np.median(lum(actual)[valid]))
    rows.append({'region':region['region'],'validPixels':int(valid.sum()),'clippedOrZeroPixels':int((~valid).sum()),
        'lutPredictedMedianLinearDisplay':wanted,'actualWorldMedianLinearDisplay':observed,'relativeMedianResidual':abs(observed-wanted)/wanted,
        'medianAbsoluteLinearResidual':float(np.median(error)),'p95AbsoluteLinearResidual':float(np.percentile(error,95)),
        'maximumAbsoluteLinearResidual':float(error.max())})
report={'status':'measured','scope':'CPU replay of the authored LUT from quantized independent-source RGB. This is an inference about the display transform; fog, bloom and quantization remain separate. This is not an HDR readback or a visual acceptance result.',
    'lut':{'path':str(path),'sha256':hashlib.sha256(raw).hexdigest(),'header':header},'regions':rows}
(run/'native-grade-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(rows))
