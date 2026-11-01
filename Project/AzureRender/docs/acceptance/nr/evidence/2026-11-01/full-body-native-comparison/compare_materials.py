import json
from pathlib import Path
import cv2
import numpy as np
from PIL import Image
cv2.setNumThreads(1);run=Path(__file__).resolve().parent;spec=json.loads((run/'mask-inputs.json').read_text());assert json.loads((run/'mask-capture-summary.json').read_text())['status']=='captured-inputs-unchanged';load=lambda p:np.asarray(Image.open(p).convert('RGB'));weights=np.array([.2126,.7152,.0722],np.float32);linear=lambda a:np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)
def ssim(a,b):
 a=a@weights;b=b@weights;blur=lambda x:cv2.GaussianBlur(x,(11,11),1.5);ma=blur(a);mb=blur(b);va=blur(a*a)-ma*ma;vb=blur(b*b)-mb*mb;cov=blur(a*b)-ma*mb
 return ((2*ma*mb+.01**2)*(2*cov+.03**2))/((ma*ma+mb*mb+.01**2)*(va+vb+.03**2))
def masks(frame):
 a=load(run/'mask-captures/material-ids/frames'/f'frame_{frame:06d}.png');result={}
 for key,p in spec['palette'].items():
  if p['excluded']:continue
  color=np.asarray(p['linearColor']);encoded=np.rint(np.where(color<=.0031308,color*12.92,1.055*color**(1/2.4)-.055)*255).astype(np.int16);m=np.max(np.abs(a.astype(np.int16)-encoded),axis=2)<=1;m=cv2.erode(m.astype(np.uint8),np.ones((13,13),np.uint8)).astype(bool);result[key]=m
 return result
def compare(reference,actual,selected):
 a=reference.astype(np.float32)/255;b=actual.astype(np.float32)/255;la=linear(a);lb=linear(b);scores=ssim(a,b);rows=[]
 for key,m in selected.items():
  count=int(m.sum());row=dict(index=int(key),name=spec['palette'][key]['name'],pixels=count,coverage='measured' if count>=50 else 'insufficient-visible-interior')
  if count>=50:
   ma=float(np.median((la@weights)[m]));mb=float(np.median((lb@weights)[m]));error=abs(ma-mb)/max(ma,1e-12);score=float(scores[m].mean());row.update(medianLuminanceRelativeError=error,ssim=score,maximumLinearError=float(np.abs(la-lb)[m].max()),p95LinearError=float(np.quantile(np.max(np.abs(la-lb),axis=2)[m],.95)),passed=error<=.1 and score>=.95)
  rows.append(row)
 return rows
report=dict(status='measured',visualAcceptance='failed-user-review',scope='Opaque material interiors from a production-rendered material ID pass, six-pixel erosion; transparent, overlay, effect and hair-card geometry conservatively masks affected pixels. Original aggregate silhouette measurements are retained separately.',thresholds=dict(ssim=.95,medianLuminanceRelativeError=.1),cases=[],diagnostics=[])
for aa in [0,3]:
 for frame in [0,31,63]:
  name=f'frame_{frame:06d}.png';a=load(run/f'captures/fullbody-source-aa{aa}/frames'/name);b=load(run/f'captures/fullbody-world-aa{aa}/frames'/name);rows=compare(a,b,masks(frame));report['cases'].append(dict(aa=aa,frame=frame,materials=rows));print('PAIR',aa,frame,[(r['index'],round(r['ssim'],6),round(r['medianLuminanceRelativeError'],4)) for r in rows if r['coverage']=='measured'],flush=True)
selected=masks(0);reference=load(run/'captures/fullbody-source-aa0/frames/frame_000000.png')
for case in json.loads((run/'diagnostic-capture-summary.json').read_text())['cases']:
 assert case['status']=='captured';actual=load(run/'diagnostic-captures'/case['name']/'frames/frame_000000.png');rows=compare(reference,actual,selected);report['diagnostics'].append(dict(name=case['name'],materials=rows));print('DIAG',case['name'],[(r['index'],round(r['ssim'],6),round(r['medianLuminanceRelativeError'],4)) for r in rows if r['coverage']=='measured'],flush=True)
source_runtime=json.loads((run/'captures/fullbody-source-aa0/runtime.json').read_text());mask_runtime=json.loads((run/'mask-captures/material-ids/runtime.json').read_text());assert source_runtime['animations']==mask_runtime['animations'] and source_runtime['fixedSteps']==mask_runtime['fixedSteps'];(run/'material-comparison.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print('MEASURED')
