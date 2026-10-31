import json,runpy
from pathlib import Path
import numpy as np
run=Path(__file__).resolve().parent
context=runpy.run_path(str(run/'fit_display_energy.py'))
regions=context['regions'];graded=context['graded'];reference=context['reference'];linear=context['linear'];luma=context['luma']
samples=[]
for region in regions:
    x0,y0,x1,y1=region['xyxy'];pixels=reference[y0:y1,x0:x1].reshape(-1,3)
    valid=(pixels.max(axis=1)<250)&(pixels.min(axis=1)>8)
    target=linear(pixels[valid][::4]);samples.append((region['hdr'],target,region['target']))
def residual(parameters):
    gain=np.exp2(parameters[:3]);exponent=2**parameters[3]
    return np.concatenate([((graded(np.maximum(hdr,0)**exponent*gain)-target)/(max(median,.02)*np.sqrt(len(target)))).ravel() for hdr,target,median in samples])
parameters=np.array([-2.,-2.,-2.,0.]);damping=.01;history=[]
for iteration in range(70):
    r=residual(parameters);loss=float(r@r)
    jacobian=np.stack([(residual(parameters+np.eye(4)[i]*.005)-residual(parameters-np.eye(4)[i]*.005))/.01 for i in range(4)],axis=1)
    step=np.linalg.solve(jacobian.T@jacobian+np.eye(4)*damping,-jacobian.T@r);step=np.clip(step,-.4,.4)
    candidate=np.clip(parameters+step,[-5,-5,-5,-2],[2,2,2,2]);r2=residual(candidate);loss2=float(r2@r2)
    accepted=loss2<loss;history.append({'iteration':iteration,'loss':loss,'candidateLoss':loss2,'accepted':accepted,'parameters':parameters.tolist()})
    if accepted:parameters=candidate;damping=max(damping*.5,1e-7)
    else:damping=min(damping*4,1e6)
    if np.max(abs(step))<1e-7:break
gain=np.exp2(parameters[:3]);exponent=2**parameters[3];results=[]
for region,(hdr,target,median) in zip(regions,samples):
    predicted=graded(np.maximum(hdr,0)**exponent*gain);measured=float(np.median(predicted@luma))
    results.append({'region':region['name'],'targetMedian':median,'predictedMedian':measured,
        'relativeMedianError':abs(measured-median)/max(median,1e-6),'meanAbsoluteLinearError':float(np.abs(predicted-target).mean())})
report={'status':'cpu-output-curve-hypothesis-gpu-verification-required',
    'scope':'A single shared monotonic outgoing-radiance response before scene grading. Reference LDR inference excludes clipped and near-zero pixels. Native HDR and whole-image qualification remain separate.',
    'linearGain':gain.tolist(),'exponent':exponent,'regions':results,
    'maximumRegionMedianLuminanceError':max(x['relativeMedianError'] for x in results),
    'predictedBudgetPass':max(x['relativeMedianError'] for x in results)<=.1,'history':history}
(run/'display-curve-fit.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='history'},indent=2))
