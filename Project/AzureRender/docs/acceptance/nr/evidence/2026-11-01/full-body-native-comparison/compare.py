import argparse,hashlib,json
from pathlib import Path
import cv2
import numpy as np
from PIL import Image,ImageDraw,ImageFont
cv2.setNumThreads(1);root=Path.cwd();run=Path(__file__).resolve().parent;parser=argparse.ArgumentParser();parser.add_argument('--aa',type=int,choices=[0,3]);args=parser.parse_args();summary=json.loads((run/'capture-summary.json').read_text());assert args.aa is not None or summary['status']=='captured-inputs-unchanged';read=lambda p:np.asarray(Image.open(p).convert('RGB'));weights=np.array([.2126,.7152,.0722],np.float32);linear=lambda a:np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)
def score(a,b):
 a=a@weights;b=b@weights;blur=lambda x:cv2.GaussianBlur(x,(11,11),1.5);ma=blur(a);mb=blur(b);va=blur(a*a)-ma*ma;vb=blur(b*b)-mb*mb;cov=blur(a*b)-ma*mb
 return ((2*ma*mb+.01**2)*(2*cov+.03**2))/((ma*ma+mb*mb+.01**2)*(va+vb+.03**2))
report=dict(status='measured',visualAcceptance='failed-user-review',scope='All standalone silhouette interior pixels, eroded by six pixels; four equal-height bands of the frozen frame-zero silhouette bounds; Gaussian SSIM on sRGB luminance.',thresholds=dict(staticSsim=.95,medianLinearLuminanceRelativeError=.10),cases=[]);images=[];font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',23)
for aa in ([args.aa] if args.aa is not None else [0,3]):
 assert all(next(c for c in summary['cases'] if c['name']==f'fullbody-{kind}-aa{aa}')['status']=='captured' for kind in ['world','source'])
 base=run/'captures'/f'fullbody-source-aa{aa}';world=run/'captures'/f'fullbody-world-aa{aa}';a0=read(base/'frames/frame_000000.png');bg=np.median(np.concatenate([a0[0],a0[-1],a0[:,0],a0[:,-1]]),axis=0).astype(np.int16);initial=np.max(np.abs(a0.astype(np.int16)-bg),axis=2)>2;assert np.mean(~initial)>.30,'Background is not sufficiently uniform for this mask method';ys,xs=np.where(initial);bounds=[int(xs.min()),int(ys.min()),int(xs.max()+1),int(ys.max()+1)];x0,y0,x1,y1=bounds;assert y1-y0>900 and initial.mean()<.60,(bounds,initial.mean());regions=[('all',[0,0,1536,1536])]+[(f'band-{i}',[x0,y0+(y1-y0)*i//4,x1,y0+(y1-y0)*(i+1)//4]) for i in range(4)];rows=[]
 for frame in range(64):
  name=f'frame_{frame:06d}.png';raw=read(base/'frames'/name);braw=read(world/'frames'/name);mask=(np.max(np.abs(raw.astype(np.int16)-bg),axis=2)>2).astype(np.uint8);mask=cv2.erode(mask,np.ones((13,13),np.uint8)).astype(bool);a=raw.astype(np.float32)/255;b=braw.astype(np.float32)/255;la=linear(a);lb=linear(b);error=np.max(np.abs(la-lb),axis=2);ssim=score(a,b);region_rows=[]
  for name,rect in regions:
   rx0,ry0,rx1,ry1=rect;selected=np.zeros(mask.shape,bool);selected[ry0:ry1,rx0:rx1]=mask[ry0:ry1,rx0:rx1];count=int(selected.sum());assert count>50,(frame,name,count);lum_a=float(np.median((la@weights)[selected]));lum_b=float(np.median((lb@weights)[selected]));relative=abs(lum_a-lum_b)/max(lum_a,1e-12);value=float(ssim[selected].mean());region_rows.append(dict(region=name,bounds=rect,pixels=count,medianLinearLuminanceReference=lum_a,medianLinearLuminanceWorld=lum_b,medianLuminanceRelativeError=relative,ssim=value,maximumLinearError=float(error[selected].max()),p95LinearError=float(np.quantile(error[selected],.95)),passed=relative<=.10 and value>=.95))
  rows.append(dict(frame=frame,regions=region_rows))
  if frame in [0,31,63]:
   Image.fromarray(mask.astype(np.uint8)*255).save(run/f'mask-aa{aa}-{frame:02d}.png');heat=np.zeros_like(raw);heat[:,:,0]=np.clip(error*255*4,0,255).astype(np.uint8);heat[~mask]=0;Image.fromarray(heat).save(run/f'error-aa{aa}-{frame:02d}.png');images.append((aa,frame,base/'frames'/f'frame_{frame:06d}.png',world/'frames'/f'frame_{frame:06d}.png'))
 source_runtime=json.loads((base/'runtime.json').read_text());world_runtime=json.loads((world/'runtime.json').read_text());assert source_runtime['animations']==world_runtime['animations'];assert source_runtime['fixedSteps']==world_runtime['fixedSteps'];assert source_runtime['animations'][0]['time']>0.9
 row=dict(aa=aa,backgroundRgb=bg.tolist(),frameZeroBounds=bounds,animation=source_runtime['animations'],fixedSteps=source_runtime['fixedSteps'],rows=rows,allInteriorCriteriaPassed=all(r['passed'] for f in rows for r in f['regions']),minimumSsim=min(r['ssim'] for f in rows for r in f['regions']),maximumMedianLuminanceRelativeError=max(r['medianLuminanceRelativeError'] for f in rows for r in f['regions']));report['cases'].append(row);print(json.dumps({k:v for k,v in row.items() if k not in {'rows','animation'}}),flush=True)
canvas=Image.new('RGB',(1800,1100),(20,24,28));draw=ImageDraw.Draw(canvas)
for i,(aa,frame,a,b) in enumerate(images):
 x=(i%3)*600;y=(i//3)*550;draw.text((x+8,y+4),f'AA{aa} 第{frame}帧：独立 / 联合',font=font,fill='white')
 for j,p in enumerate([a,b]):canvas.paste(Image.open(p).convert('RGB').resize((300,300)),(x+j*300,y+40))
 canvas.paste(Image.open(run/f'error-aa{aa}-{frame:02d}.png').resize((200,200)),(x+200,y+345))
suffix='' if args.aa is None else f'-aa{args.aa}';canvas.save(run/f'fullbody-comparison{suffix}.png');(run/f'comparison{suffix}.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print('MEASURED')
