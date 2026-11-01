import hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
run=Path(__file__).resolve().parent;source=json.loads((run/'variant-targets.json').read_text());assert source['status']=='exported-variant-targets'
rows=[];panels=[]
def decode(words):
 channels=[]
 for shift,bits in [(0,6),(11,6),(22,5)]:
  packed=(words>>shift)&((1<<(bits+5))-1);exponent=packed>>bits;mantissa=packed&((1<<bits)-1)
  channel=np.ldexp(1+mantissa.astype(float)/(1<<bits),exponent.astype(int)-15)
  channels.append(np.where(exponent==0,np.ldexp(mantissa.astype(float),-14-bits),channel))
 return np.stack(channels,2)
for draw in source['draws']:
 before,after=draw['outputs'];assert before['format']==after['format']=='R11G11B10_FLOAT';w,h=after['width'],after['height']
 arrays=[np.frombuffer((run/'variant-targets'/v['path']).read_bytes(),dtype='<u4').reshape(h,w)[::-1] for v in [before,after]]
 mask=arrays[0]!=arrays[1];ys,xs=np.where(mask);assert len(xs)>0
 interior=mask.copy();interior[:3]=False;interior[-3:]=False;interior[:,:3]=False;interior[:,-3:]=False
 for dy in range(-2,3):
  for dx in range(-2,3):interior&=np.roll(np.roll(mask,dy,0),dx,1)
 y,x=np.where(interior);assert len(x)>0
 points=[]
 for fraction in [.2,.5,.8]:
  seed=np.array([np.quantile(x,fraction),np.median(y)])
  index=np.argmin((x-seed[0])**2+(y-seed[1])**2);points.append(dict(x=int(x[index]),y=int(y[index]),nativeY=h-1-int(y[index])))
 row=dict(event=draw['event'],shaderSha256=draw['shader']['sha256'],changedPixels=int(len(xs)),interiorPixels=int(len(x)),bounds=[int(xs.min()),int(ys.min()),int(xs.max()),int(ys.max())],points=points);rows.append(row)
 rgb=decode(arrays[1])*.25;rgb=np.where(rgb<=.0031308,rgb*12.92,1.055*np.maximum(rgb,0)**(1/2.4)-.055);rgb=np.uint8(np.rint(np.clip(rgb,0,1)*255))
 image=Image.fromarray(rgb);image.save(run/('native-after-%d.png'%draw['event']));Image.fromarray(np.uint8(mask)*255).save(run/('native-mask-%d.png'%draw['event']))
 dim=np.uint8(rgb*.22);dim[mask]=rgb[mask];panel=Image.fromarray(dim).resize((768,432));canvas=Image.new('RGB',(768,460),'#181c24');canvas.paste(panel,(0,28));ImageDraw.Draw(canvas).text((10,7),'Native draw %d: changed surface highlighted'%draw['event'],fill='white');panels.append(canvas)
sheet=Image.new('RGB',(1536,920));[sheet.paste(p,((i%2)*768,(i//2)*460)) for i,p in enumerate(panels)];sheet.save(run/'native-variant-contact.png')
(run/'variant-pixel-selection.json').write_bytes((json.dumps(dict(status='selected-native-draw-interiors',flipY=True,draws=rows),indent=2)+'\n').encode())
print(json.dumps(rows))
