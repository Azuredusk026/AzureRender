import hashlib,json
from pathlib import Path
import cv2
import numpy as np
from PIL import Image,ImageDraw,ImageFont
cv2.setNumThreads(1)
root=Path.cwd();run=Path(__file__).resolve().parent;prior=root/'build/runs/20261101-043757-material-display-transform-93d6e0';summary=json.loads((run/'standalone-capture-summary.json').read_text());assert summary['status']=='captured-inputs-unchanged'
source=json.loads((prior/'native-final-comparison.json').read_text());regions=[row for row in source['comparisons'][0]['regions'] if not row['region'].startswith('environment-')];assert len(regions)==5
world=prior/'native-final-captures/near-world-linear/frames/frame_000000.png';read=lambda p:np.asarray(Image.open(p).convert('RGB'),dtype=np.float64)/255;linear=lambda x:np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4);weights=np.array([.2126,.7152,.0722]);b=read(world);report=dict(status='measured',scope='Five frozen character interior rectangles. Geometry-only preserves world lights and display; source-standalone uses the original independent scene settings, ten actor lights, one character resource and no environment geometry.',worldImageSha256=hashlib.sha256(world.read_bytes()).hexdigest(),cases=[],visualAcceptance='failed-user-review')
def ssim(a,b):
 a=a@weights;b=b@weights;blur=lambda x:cv2.GaussianBlur(x,(11,11),1.5);ma=blur(a);mb=blur(b);va=blur(a*a)-ma*ma;vb=blur(b*b)-mb*mb;cov=blur(a*b)-ma*mb
 values=((2*ma*mb+.01**2)*(2*cov+.03**2))/((ma*ma+mb*mb+.01**2)*(va+vb+.03**2));return float(np.mean(values[5:-5,5:-5]))
paths=[]
for case in summary['cases']:
 path=run/'standalone-captures'/case['name']/'frames/frame_000000.png';a=read(path);rows=[]
 for region in regions:
  x0,y0,x1,y1=region['bounds'];aa=a[y0:y1,x0:x1];bb=b[y0:y1,x0:x1];la=linear(aa);lb=linear(bb);ma=float(np.median(la@weights));mb=float(np.median(lb@weights));error=abs(ma-mb)/max(ma,1e-12);score=ssim(aa,bb)
  rows.append(dict(region=region['region'],bounds=region['bounds'],referenceMedianLinearLuminance=ma,worldMedianLinearLuminance=mb,medianLuminanceRelativeError=error,staticLuminanceSsim=score,maximumLinearError=float(np.max(np.abs(la-lb))),passed=error<=.10 and score>=.95))
 report['cases'].append(dict(name=case['name'],imageSha256=hashlib.sha256(path.read_bytes()).hexdigest(),regions=rows,interiorCriteriaPassed=all(row['passed'] for row in rows)));paths.append(path)
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',24);canvas=Image.new('RGB',(1800,640),(20,24,28));draw=ImageDraw.Draw(canvas)
for i,(path,label) in enumerate(zip([world,*paths],['完整联合场景','仅角色几何与世界显示','原独立照明与独立显示'])):
 canvas.paste(Image.open(path).convert('RGB').crop((340,260,1190,1110)).resize((600,600)),(600*i,40));draw.text((600*i+12,4),label,font=font,fill='white')
canvas.save(run/'standalone-comparison.png');(run/'standalone-comparison.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report))
