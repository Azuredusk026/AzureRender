import hashlib,json
from pathlib import Path
import cv2
import numpy as np
from PIL import Image
cv2.setNumThreads(1);root=Path.cwd();run=Path(__file__).resolve().parent;prior=root/'build/runs/20261101-043757-material-display-transform-93d6e0';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();record=dict(status='running',visualAcceptance='failed-user-review',checks=[])
for name,count in [('full-world-hidden',12),('near-default-compatibility',1)]:
 current=run/'shader-family-revision'/'native-captures'/name/'frames';old=prior/'native-final-captures'/name/'frames';assert len(list(current.glob('frame_*.png')))==count;differences=[i for i in range(count) if sha(current/f'frame_{i:06d}.png')!=sha(old/f'frame_{i:06d}.png')];record['checks'].append(dict(name=name,frames=count,exactFrames=count-len(differences),differentFrames=differences));print(name,count-len(differences),'exact',flush=True)
source=root/'build/runs/20261101-082926-joint-playable-display-c26c1b/standalone-captures/source-standalone/frames/frame_000000.png';actual=run/'native-captures/near-world-linear/frames/frame_000000.png';load=lambda p:np.asarray(Image.open(p).convert('RGB'),np.float32)/255;a=load(source);b=load(actual);linear=lambda x:np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4);weights=np.array([.2126,.7152,.0722],np.float32);regions=json.loads((prior/'native-final-comparison.json').read_text())['comparisons'][0]['regions'];rows=[]
for region in regions:
 if region['region'].startswith('environment-'):continue
 x0,y0,x1,y1=region['bounds'];aa=a[y0:y1,x0:x1];bb=b[y0:y1,x0:x1];ay=aa@weights;by=bb@weights;blur=lambda x:cv2.GaussianBlur(x,(11,11),1.5);ma=blur(ay);mb=blur(by);ssim=((2*ma*mb+.01**2)*(2*(blur(ay*by)-ma*mb)+.03**2))/((ma*ma+mb*mb+.01**2)*(blur(ay*ay)-ma*ma+blur(by*by)-mb*mb+.03**2));va=float(np.median(linear(aa)@weights));vb=float(np.median(linear(bb)@weights));error=abs(va-vb)/max(va,1e-12);score=float(ssim[5:-5,5:-5].mean());rows.append(dict(region=region['region'],bounds=region['bounds'],ssim=score,medianLuminanceRelativeError=error,passed=score>=.95 and error<=.1))
record['headRegions']=rows;record['headCriteriaPassed']=all(r['passed'] for r in rows);record['strictCompatibilityPassed']=all(not r['differentFrames'] for r in record['checks']);record['status']='measured';(run/'shader-family-revision'/'compatibility-comparison.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps(record))
