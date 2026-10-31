import hashlib,json,struct
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent
source_run=root/'build/runs/20261101-021345-material-radiance-normalization-1e6231'
lut=source_run/'project/native-gi/captured-3534.azlut';raw=lut.read_bytes();length=struct.unpack_from('<I',raw,8)[0]
header=json.loads(raw[12:12+length]);n=header['dimension'];table=np.frombuffer(raw,dtype='<f4',offset=12+length).reshape(n,n,n,4)
source=source_run/'native-captures/near-source-isolated/frames/frame_000000.png';image=np.array(Image.open(source).convert('RGB'))
regions=json.loads((root/'build/runs/20261101-014745-joint-display-domain-diagnosis-8779a0/component-comparison.json').read_text())['pairs'][0]['regions'][:5]
targets=[];labels=[]
for region in regions:
    x,y,z,w=region['bounds'];pixels=image[y:w,x:z].reshape(-1,3);pixels=pixels[np.linspace(0,len(pixels)-1,100).astype(int)]
    targets.extend(pixels);labels.extend([region['region']]*len(pixels))
encoded=np.array(targets);values=encoded.astype(float)/255;targets=np.where(values<=.04045,values/12.92,((values+.055)/1.055)**2.4)
zero=float(np.log2(.047995999455451965)*.3010300099849701*.244160994887352+.3860360085964203)
def evaluate(coordinates):
    coord=coordinates*(n-1);lo=np.floor(coord).astype(int);hi=np.minimum(lo+1,n-1);f=coord-lo;result=np.zeros_like(coordinates)
    for corner in range(8):
        mask=np.array([(corner>>axis)&1 for axis in range(3)]);cell=np.where(mask,hi,lo)
        weight=np.prod(np.where(mask,f,1-f),axis=-1)
        result+=table[cell[:,1],cell[:,2],cell[:,0],:3]*weight[:,None]
    return result
g,b,r=np.meshgrid(np.arange(n),np.arange(n),np.arange(n),indexing='ij');grid=np.column_stack([r.ravel(),g.ravel(),b.ravel()])/(n-1)
valid=np.all(grid>=zero,axis=1);grid=grid[valid];colors=table[...,:3].reshape(-1,3)[valid].astype(float)
position=np.empty_like(targets)
for start in range(0,len(targets),32):
    batch=targets[start:start+32];distance=np.sum(colors*colors,axis=1)[:,None]+np.sum(batch*batch,axis=1)[None,:]-2*colors@batch.T
    position[start:start+len(batch)]=grid[np.argmin(distance,axis=0)]
best=evaluate(position);loss=np.sum((best-targets)**2,axis=1);trace=[]
for iteration in range(60):
    jacobian=np.zeros((len(targets),3,3));step=1e-4
    for axis in range(3):
        low=position.copy();high=position.copy();low[:,axis]=np.maximum(zero,position[:,axis]-step);high[:,axis]=np.minimum(1,position[:,axis]+step)
        jacobian[:,:,axis]=(evaluate(high)-evaluate(low))/(high[:,axis]-low[:,axis])[:,None]
    jt=np.swapaxes(jacobian,1,2)
    delta=np.linalg.solve(jt@jacobian+np.eye(3)[None,:,:]*1e-8,(jt@(targets-best)[...,None]))[...,0]
    delta=np.clip(delta,-.1,.1);chosen=position.copy();chosen_color=best.copy();chosen_loss=loss.copy()
    for factor in [1,.5,.25,.125,.0625,.03125]:
        candidate=np.clip(position+delta*factor,zero,1);color=evaluate(candidate);error=np.sum((color-targets)**2,axis=1);improved=error<chosen_loss
        chosen[improved]=candidate[improved];chosen_color[improved]=color[improved];chosen_loss[improved]=error[improved]
    position,best,loss=chosen,chosen_color,chosen_loss
    trace.append({'iteration':iteration,'maximumRgbError':float(np.abs(best-targets).max()),'medianLoss':float(np.median(loss))})
rows=[]
for region in regions:
    mask=np.array(labels)==region['region'];errors=np.abs(best[mask]-targets[mask]);a=targets[mask]@np.array([.2126,.7152,.0722]);b=best[mask]@np.array([.2126,.7152,.0722])
    rows.append({'region':region['region'],'samples':int(mask.sum()),'maximumRgbError':float(errors.max()),'medianMaximumRgbError':float(np.median(errors.max(axis=1))),
        'samplesOutsidePoint008':int((errors.max(axis=1)>.008).sum()),'medianRelativeLuminanceError':float(np.median(np.abs(b-a)/np.maximum(a,1e-6)))})
inputs={'lutSha256':hashlib.sha256(raw).hexdigest(),'imageSha256':hashlib.sha256(source.read_bytes()).hexdigest(),'sourceImage':str(source),'lut':str(lut)}
report={'status':'measured','scope':'Constrained numeric feasibility only. No shader, project material or acceptance budget is changed. Values are sampled source display RGB; a nearest-lattice seed and damped local inversion estimate attainable scene-LUT colors. Local convergence does not prove a global impossibility.',
    'inputs':inputs,'regions':rows,'trace':trace,'samples':[{'region':label,'displayRgb':desired.tolist(),'attainedRgb':actual.tolist(),'inputLogCoordinates':coordinate.tolist()} for label,desired,actual,coordinate in zip(labels,targets,best,position)]}
(run/'inverse-feasibility.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(rows))
