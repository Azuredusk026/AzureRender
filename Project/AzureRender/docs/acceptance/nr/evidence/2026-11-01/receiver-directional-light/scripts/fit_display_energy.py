import hashlib,json,struct
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent
reference_path=run/'character-capture/near-reference-source-display/frames/frame_000000.png'
current_path=run/'character-capture/near-reference-light/frames/frame_000000.png'
lut_path=run/'project/native-gi/captured-3534.azlut'
data=lut_path.read_bytes();assert data[:8]==b'AZLUT001'
size=struct.unpack_from('<I',data,8)[0];header=json.loads(data[12:12+size]);n=header['dimension']
assert header['inputDomain']=='logc3-full-log' and header['outputDomain']=='linear'
lut=np.frombuffer(data,dtype='<f4',offset=12+size).reshape(n,n*n,4)[:,:,:3]
def linear(rgb):
    x=np.asarray(rgb,dtype=float)/255
    return np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)
def graded(value):
    coordinate=np.clip(np.log2(np.maximum(value*5.555555820465088+.047995999455451965,1e-30))*.3010300099849701*.244160994887352+.3860360085964203,0,1)*(n-1)
    lo=np.floor(coordinate).astype(int);hi=np.minimum(lo+1,n-1);fraction=coordinate-lo
    value=np.zeros_like(coordinate)
    for r in [0,1]:
        for g in [0,1]:
            for b in [0,1]:
                at=np.stack([hi[:,0] if r else lo[:,0],hi[:,1] if g else lo[:,1],hi[:,2] if b else lo[:,2]],axis=1)
                weight=np.prod(np.where([r,g,b],fraction,1-fraction),axis=1)
                value+=lut[at[:,1],at[:,2]*n+at[:,0]]*weight[:,None]
    return np.clip(value,0,1)
reference=np.asarray(Image.open(reference_path).convert('RGB'))
current=np.asarray(Image.open(current_path).convert('RGB'))
boxes={'hair-crown':[710,400,820,550],'hair-fringe':[700,570,800,650],
    'face-cheek':[705,852,795,885],'face-chin':[770,960,810,985],'eye-left':[625,784,683,814]}
luma=np.array([.21267290413379669,.7151522040367126,.07217500358819962]);regions=[]
for name,(x0,y0,x1,y1) in boxes.items():
    ref=reference[y0:y1,x0:x1].reshape(-1,3);actual=current[y0:y1,x0:x1].reshape(-1,3)
    valid=(ref.max(axis=1)<250)&(ref.min(axis=1)>8)
    ref=ref[valid][::4];actual=actual[valid][::4]
    target=linear(ref);hdr=target/.25;predicted=graded(hdr*.25);actual=linear(actual)
    regions.append({'name':name,'xyxy':[x0,y0,x1,y1],'samples':len(ref),'hdr':hdr,
        'target':float(np.median(target@luma)),'currentMedian':float(np.median(actual@luma)),
        'linearPredictionMeanAbsoluteError':float(np.abs(predicted-actual).mean()),
        'linearPredictionMaximumError':float(np.abs(predicted-actual).max())})
trials=[]
for ev in np.linspace(-3,-.5,501):
    scale=2**ev;errors=[]
    for region in regions:
        median=float(np.median(graded(region['hdr']*scale)@luma))
        errors.append(abs(median-region['target'])/max(region['target'],1e-6))
    trials.append({'exposureEv':float(ev),'scale':scale,'maximumRegionMedianLuminanceError':max(errors),'regionErrors':errors})
best=min(trials,key=lambda row:row['maximumRegionMedianLuminanceError'])
serial=[{k:v for k,v in row.items() if k!='hdr'} for row in regions]
report={'status':'cpu-hypothesis-gpu-verification-required','regionMedianRelativeBudget':.1,
    'scope':'Same-pose source display compared with actual scene LUT. LDR inference excludes clipped pixels; specular, eye saturation and bloom can violate uniform radiance scaling.',
    'referenceImageSha256':hashlib.sha256(reference_path.read_bytes()).hexdigest(),
    'currentImageSha256':hashlib.sha256(current_path.read_bytes()).hexdigest(),
    'lutSha256':hashlib.sha256(data).hexdigest(),'regions':serial,'best':best,'predictedBudgetPass':best['maximumRegionMedianLuminanceError']<=.1}
(run/'display-energy-fit.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
