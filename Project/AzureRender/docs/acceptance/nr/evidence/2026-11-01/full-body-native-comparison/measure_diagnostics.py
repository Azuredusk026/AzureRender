import json
from pathlib import Path
import cv2
import numpy as np
from PIL import Image
cv2.setNumThreads(1);run=Path(__file__).resolve().parent;load=lambda p:np.asarray(Image.open(p).convert('RGB'),np.float32)/255;linear=lambda a:np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4);weights=np.array([.2126,.7152,.0722],np.float32);reference=load(run/'captures/fullbody-source-aa0/frames/frame_000000.png');world=load(run/'captures/fullbody-world-aa0/frames/frame_000000.png');mask=np.asarray(Image.open(run/'mask-aa0-00.png'))>0;regions=json.loads((run/'comparison-aa0.json').read_text())['cases'][0]['rows'][0]['regions'];rows=[]
def ssim(a,b):
 a=a@weights;b=b@weights;blur=lambda x:cv2.GaussianBlur(x,(11,11),1.5);ma=blur(a);mb=blur(b);va=blur(a*a)-ma*ma;vb=blur(b*b)-mb*mb;cov=blur(a*b)-ma*mb
 return ((2*ma*mb+.01**2)*(2*cov+.03**2))/((ma*ma+mb*mb+.01**2)*(va+vb+.03**2))
summary=json.loads((run/'diagnostic-capture-summary.json').read_text())
for case in summary['cases']:
 if case['status']!='captured':continue
 name=case['name'];actual=load(run/'diagnostic-captures'/name/'frames/frame_000000.png');score=ssim(reference,actual);error=np.max(np.abs(linear(actual)-linear(world)),axis=2);details=[]
 for r in regions:
  x0,y0,x1,y1=r['bounds'];selected=np.zeros(mask.shape,bool);selected[y0:y1,x0:x1]=mask[y0:y1,x0:x1];ma=float(np.median((linear(reference)@weights)[selected]));mb=float(np.median((linear(actual)@weights)[selected]));details.append(dict(region=r['region'],ssim=float(score[selected].mean()),medianLuminanceRelativeError=abs(ma-mb)/max(ma,1e-12),p95LinearResponse=float(np.quantile(error[selected],.95)),maximumLinearResponse=float(error[selected].max())))
 row=dict(name=name,regions=details);rows.append(row);print(json.dumps(row),flush=True)
(run/'diagnostic-measurement.json').write_bytes((json.dumps(dict(status='measured',captureStatus=summary['status'],cases=rows,visualAcceptance='failed-user-review'),indent=2)+'\n').encode())
