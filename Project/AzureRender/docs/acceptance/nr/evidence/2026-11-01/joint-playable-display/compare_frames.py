import json
from pathlib import Path
import cv2
import numpy as np
from PIL import Image
cv2.setNumThreads(1)
run=Path(__file__).resolve().parent;hashes={cfg:json.loads((run/('route-'+cfg)/'frame-fingerprints.json').read_text()) for cfg in ['release','debug']}
linear=lambda x:np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)
weights=np.array([.2126,.7152,.0722],dtype=np.float64)
def ssim(a,b):
 a=a@weights;b=b@weights;blur=lambda x:cv2.GaussianBlur(x,(11,11),1.5)
 ma=blur(a);mb=blur(b);va=blur(a*a)-ma*ma;vb=blur(b*b)-mb*mb;cov=blur(a*b)-ma*mb
 values=((2*ma*mb+.01**2)*(2*cov+.03**2))/((ma*ma+mb*mb+.01**2)*(va+vb+.03**2))
 return float(np.mean(values[5:-5,5:-5]))
rows=[]
for name,digest in hashes['release'].items():
 if digest==hashes['debug'][name]:continue
 a=np.asarray(Image.open(run/'route-release/frames'/name).convert('RGB'),dtype=np.float64)/255
 b=np.asarray(Image.open(run/'route-debug/frames'/name).convert('RGB'),dtype=np.float64)/255
 changed=np.any(a!=b,axis=2);la=linear(a);lb=linear(b);medianA=float(np.median(la@weights));medianB=float(np.median(lb@weights))
 ys,xs=np.where(changed)
 rows.append(dict(frame=int(name[6:12]),changedPixels=int(changed.sum()),changedPixelFraction=float(changed.mean()),maximumLinearError=float(np.max(np.abs(la-lb))),meanAbsoluteLinearError=float(np.mean(np.abs(la-lb))),medianLuminanceRelativeError=abs(medianA-medianB)/max(medianA,1e-12),srgbLuminanceSsim=ssim(a,b),bounds=[int(xs.min()),int(ys.min()),int(xs.max()+1),int(ys.max()+1)]))
report=dict(status='measured',scope='All 540 engine configuration pairs. These diagnostics are separate from calibrated native references, exact compatibility controls and material interior pixel-oracle budgets.',exactImageFiles=540-len(rows),differentImageFiles=len(rows),maximumChangedPixels=max(row['changedPixels'] for row in rows),maximumChangedPixelFraction=max(row['changedPixelFraction'] for row in rows),maximumLinearError=max(row['maximumLinearError'] for row in rows),minimumSrgbLuminanceSsim=min(row['srgbLuminanceSsim'] for row in rows),maximumMedianLuminanceRelativeError=max(row['medianLuminanceRelativeError'] for row in rows),rows=rows,visualAcceptance='failed-user-review')
(run/'configuration-image-diagnostics.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in report.items() if k!='rows'}))
