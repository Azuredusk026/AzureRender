import hashlib,json,math
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
run=Path(__file__).resolve().parent;root=Path.cwd();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
reports={cfg:json.loads((run/('route-'+cfg)/'runtime.json').read_text()) for cfg in ['release','debug']}
for cfg in reports:
 assert json.loads((run/('route-'+cfg)/'summary.json').read_text())['status']=='behavior-passed-visual-review-pending'
 assert json.loads((run/('route-'+cfg)/'behavior-verification.json').read_text())['status']=='passed'
 frames=sorted((run/('route-'+cfg)/'frames').glob('frame_*.png'));assert len(frames)==540
 (run/('route-'+cfg)/'frame-fingerprints.json').write_bytes((json.dumps({p.name:sha(p) for p in frames},indent=2)+'\n').encode())
pose_error=max(math.dist(a['position'],b['position']) for ra,rb in zip(reports['release']['routeFrames'],reports['debug']['routeFrames']) for a,b in zip(ra['characters'],rb['characters']))
assert pose_error<=.0001
selected=[0,100,190,299,360,410,451,539];canvas=Image.new('RGB',(1920,1152),(20,24,28));draw=ImageDraw.Draw(canvas);font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',20)
labels=['出生待机','行走','冲刺','失焦释放','转动相机','转身前进','重开','重开后待机'];comparisons=[]
linear=lambda x:np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)
for i,(frame,label) in enumerate(zip(selected,labels)):
 path=run/'route-release/frames'/('frame_'+str(frame).zfill(6)+'.png');other=run/'route-debug/frames'/path.name
 a=np.asarray(Image.open(path).convert('RGB'),dtype=np.float64)/255;b=np.asarray(Image.open(other).convert('RGB'),dtype=np.float64)/255
 comparisons.append(dict(frame=frame,maximumLinearError=float(np.max(np.abs(linear(a)-linear(b)))),exactPixelFraction=float(np.mean(np.all(a==b,axis=2)))))
 x=(i%4)*480;y=(i//4)*576;draw.text((x+8,y+4),label+' '+str(frame),font=font,fill='white');canvas.paste(Image.open(path).convert('RGB').resize((480,270)),(x,y+36))
 # Keep a native-size actor and immediate floor crop under each full view.
 canvas.paste(Image.open(path).convert('RGB').crop((240,150,720,420)),(x,y+306))
canvas.save(run/'playable-route-contact.png')
record=dict(status='behavior-cross-configuration-passed',visualAcceptance='failed-user-review',characterPositionMaximumConfigurationDifference=pose_error,selectedFrameDiagnostics=comparisons,selectedFrameScope='Configuration diagnostics use identical production inputs and fixed frame indices. Whole-body and dynamic visual acceptance remain separate.',programs={cfg:json.loads((run/('route-'+cfg)/'summary.json').read_text())['programSha256'] for cfg in reports})
(run/'review.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps(record))
