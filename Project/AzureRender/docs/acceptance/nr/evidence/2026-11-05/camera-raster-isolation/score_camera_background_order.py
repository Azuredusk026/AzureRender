from pathlib import Path
import numpy as np,cv2,json,gzip
from PIL import Image
r=Path(__file__).resolve().parent;cv2.setNumThreads(1);reference=np.asarray(Image.open(r/'full-character-stage-reference.png').convert('RGB')).astype(float);owner=np.zeros((1440,2560),np.uint16)
for row in json.loads((r/'post-opaque-ownership/manifest.json').read_bytes())['events']:
 before,after=[np.frombuffer(gzip.decompress((r/'post-opaque-ownership'/n).read_bytes()),'<u4').reshape(1440,2560)[::-1] for n in row['files']];owner[before!=after]=row['event']
sourceHair=np.asarray(Image.open(r.parent/'20261023-105023-character-composite-contribution-diagnosis-acb8e5/native-hair-visible-mask.png'))>0;fullHair=sourceHair|np.isin(owner,[1138,1291,1314]);weights=np.array([.2126,.7152,.0722]);linear=lambda v:np.where(v<=.04045,v/12.92,((v+.055)/1.055)**2.4);blur=lambda x:cv2.GaussianBlur(x,(11,11),1.5);x=reference/255;ax=x@weights;ma=blur(ax);lx=linear(x)@weights;report={}
for cfg in ['baseline','captured-matrix']:
 frame=r/({'baseline':'snapshot-shell-order-regression/release-background','captured-matrix':'captured-camera-background-order-capture'}[cfg])/'frames/frame_000000.png'
 if not frame.exists():continue
 y=np.asarray(Image.open(frame).convert('RGB')).astype(float)/255;by=y@weights;mb=blur(by);ly=linear(y)@weights;ssim=((2*ma*mb+.01**2)*(2*(blur(ax*by)-ma*mb)+.03**2))/((ma*ma+mb*mb+.01**2)*(blur(ax*ax)-ma*ma+blur(by*by)-mb*mb+.03**2));delta=np.abs(y*255-reference).max(2);cases=[]
 for name,mask in [('source-hair-visible',sourceHair),('hair-opaque-owner',sourceHair&(owner==0)),('hair-complete-stages',fullHair),('hair-outline',owner==1138),('hair-front-blend',owner==1291),('hair-back-blend',owner==1314)]:
  pixels=int(mask.sum());score=float(ssim[mask].mean());lum=abs(float(np.median(ly[mask]))-float(np.median(lx[mask])))/max(float(np.median(lx[mask])),1e-12);cases.append(dict(name=name,pixels=pixels,ssim=score,medianLuminanceRelativeError=lum,regionalPassed=score>=.95 and lum<=.1,pixelsOverOneByte=int((delta[mask]>1.000001).sum()),maximumByteError=float(delta[mask].max()),meanMaximumByteError=float(delta[mask].mean())))
 report[cfg]=cases
(r/'captured-camera-background-complete-hair.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:next(v for v in rows if v['name']=='hair-complete-stages') for k,rows in report.items()},indent=2))
