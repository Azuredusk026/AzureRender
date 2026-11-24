import argparse,json
from pathlib import Path
import cv2
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageOps
cv2.setNumThreads(1);run=Path(__file__).resolve().parent;prior=Path(json.loads((run/'candidate.json').read_text())['referenceRun']);parser=argparse.ArgumentParser();parser.add_argument('--aa',type=int,choices=[0,3],required=True);args=parser.parse_args();aa=args.aa;load=lambda p:np.asarray(Image.open(p).convert('RGB'));weights=np.array([.2126,.7152,.0722],np.float32);linear=lambda a:np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4);palette=json.loads((prior/'mask-inputs.json').read_text())['palette'];rows=[]
def compare(a,b,identity):
 a=a.astype(np.float32)/255;b=b.astype(np.float32)/255;la=linear(a);lb=linear(b);ay=a@weights;by=b@weights;blur=lambda x:cv2.GaussianBlur(x,(11,11),1.5);ma=blur(ay);mb=blur(by);ssim=((2*ma*mb+.01**2)*(2*(blur(ay*by)-ma*mb)+.03**2))/((ma*ma+mb*mb+.01**2)*(blur(ay*ay)-ma*ma+blur(by*by)-mb*mb+.03**2));result=[]
 for key,p in palette.items():
  if p['excluded']:continue
  color=np.array(p['linearColor']);encoded=np.rint(np.where(color<=.0031308,color*12.92,1.055*color**(1/2.4)-.055)*255).astype(np.int16);mask=cv2.erode((np.max(np.abs(identity.astype(np.int16)-encoded),axis=2)<=1).astype(np.uint8),np.ones((13,13),np.uint8)).astype(bool);count=int(mask.sum());row=dict(index=int(key),name=p['name'],pixels=count,coverage='measured' if count>=50 else 'insufficient-visible-interior')
  if count>=50:
   va=float(np.median((la@weights)[mask]));vb=float(np.median((lb@weights)[mask]));error=abs(va-vb)/max(va,1e-12);score=float(ssim[mask].mean());row.update(ssim=score,medianLuminanceRelativeError=error,maximumLinearError=float(np.abs(la-lb)[mask].max()),p95LinearError=float(np.quantile(np.max(np.abs(la-lb),axis=2)[mask],.95)),passed=score>=.95 and error<=.1)
  result.append(row)
 return result
for frame in [0,31,63]:
 name=f'frame_{frame:06d}.png';identity=load(prior/'mask-captures/material-ids/frames'/name);old=load(prior/f'captures/fullbody-source-aa{aa}/frames'/name);world=load(run/'shader-family-revision'/f'native-captures/fullbody-world-aa{aa}/frames'/name);source=load(run/'shader-family-revision'/f'native-captures/fullbody-source-aa{aa}/frames'/name)
 for pair,a,b in [('world-vs-original-standalone',old,world),('world-vs-matched-standalone',source,world),('standalone-vs-original-standalone',old,source)]:
  values=compare(a,b,identity);rows.append(dict(frame=frame,pair=pair,materials=values));print(frame,pair,[(r['index'],round(r['ssim'],6),round(r['medianLuminanceRelativeError'],4)) for r in values if r['coverage']=='measured'],flush=True)
old_runtime=json.loads((prior/f'captures/fullbody-source-aa{aa}/runtime.json').read_text());world_runtime=json.loads((run/'shader-family-revision'/f'native-captures/fullbody-world-aa{aa}/runtime.json').read_text());source_runtime=json.loads((run/'shader-family-revision'/f'native-captures/fullbody-source-aa{aa}/runtime.json').read_text());assert old_runtime['animations']==world_runtime['animations']==source_runtime['animations'];assert old_runtime['fixedSteps']==world_runtime['fixedSteps']==source_runtime['fixedSteps']==63;report=dict(status='measured',antiAliasing=aa,visualAcceptance='failed-user-review',thresholds=dict(ssim=.95,medianLuminanceRelativeError=.1),cases=rows);(run/'shader-family-revision'/f'body-comparison-aa{aa}.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',26);canvas=Image.new('RGB',(1800,750),(20,24,28));draw=ImageDraw.Draw(canvas)
for i,(path,label) in enumerate([(prior/f'captures/fullbody-source-aa{aa}/frames/frame_000000.png','冻结独立入口'),(run/'shader-family-revision'/f'native-captures/fullbody-source-aa{aa}/frames/frame_000000.png','当前独立入口'),(run/'shader-family-revision'/f'native-captures/fullbody-world-aa{aa}/frames/frame_000000.png','完整联合场景')]):
 draw.text((i*600+12,8),label,font=font,fill='white');panel=ImageOps.contain(Image.open(path).convert('RGB').crop((300,260,1350,1536)),(600,700));canvas.paste(panel,(i*600+(600-panel.width)//2,50))
canvas.save(run/'shader-family-revision'/f'body-comparison-aa{aa}.png')
