import json
from pathlib import Path
import numpy as np
from PIL import Image
run=Path(__file__).resolve().parent;rows=[]
def unit(v):return v/np.linalg.norm(v)
def sample(image,uv):
 h,w=image.shape[:2];x,y=np.asarray(uv)*[w,h]-.5;i,j=np.floor([x,y]).astype(int);fx,fy=x-i,y-j
 return sum(image[np.clip(j+dy,0,h-1),np.clip(i+dx,0,w-1),:3]*wx*wy for dx,wx in [(0,1-fx),(1,fx)] for dy,wy in [(0,1-fy),(1,fy)])
for event in [910,928]:
 trace=json.loads((run/f'variant-trace-{event}.json').read_text());assert trace['complete'];registers={};snapshots={}
 wanted={1230,1239,1245,1253,1257,1260,1261,1262,1264,1273,1274,1278,1279}
 for state in trace['states']:
  for change in state['changes']:registers[change['after']['name']]=np.asarray(change['after']['float32'],dtype=float)
  if state['next_instruction'] in wanted:snapshots[state['next_instruction']]={k:v.copy() for k,v in registers.items()}
 assert wanted<=set(snapshots),sorted(wanted-set(snapshots))
 a=snapshots[1230];normal=a['r15'][:3];view=a['r2'][[1,2,3]];light=a['r23'][:3];visibility=a['r7'][3]
 cb0=np.fromfile(run/f'variant-targets/e{event}/cb0.bin',dtype='<f4').reshape(-1,4);cb1=np.fromfile(run/f'variant-targets/e{event}/cb1.bin',dtype='<f4').reshape(-1,4);cb6=np.fromfile(run/f'variant-targets/e{event}/cb6.bin',dtype='<f4').reshape(-1,4)
 camera=cb0[6,:3].astype(float).copy();camera[1]=.5+(light[1]-.5)*visibility
 half=unit(light*visibility+2*unit(camera)+view*(visibility+2));nh=np.dot(normal,half);nv=np.clip(np.dot(normal,view),0,1)
 rough=a['r3'][2];metal=a['r7'][0];alpha=max(rough*rough,1/128);alpha2=alpha*alpha;denominator=(nh*nh*(alpha2-1)+1)**2;distribution=1 if alpha2==denominator else alpha2/denominator;mode=float(cb6[1,0])
 uv=np.array([(1-mode)*distribution*(alpha2+.0001)+mode*nv*nv,rough*(1-metal)])
 image=np.asarray(Image.open(run/f'variant-targets/e{event}/t12.dds').convert('RGBA'),dtype=float)/255;ramp=sample(image,uv);f0=snapshots[1261]['r17'][:3];spec=f0*ramp;lobe=np.clip(distribution*.5/(2*nv+alpha+.0001)-2**-14,0,20)
 fd=a['r8'][3];floor=float(cb1[110,2]);radiance=snapshots[1274]['r11'][:3];main=spec*lobe*(.5+.5*fd)*(floor+(1-floor)*fd)*radiance*float(cb1[123,3])
 row=dict(event=event,inputs=dict(normal=normal.tolist(),view=view.tolist(),light=light.tolist(),visibility=float(visibility),roughness=float(rough),metallic=float(metal),iridescence=mode,fullDiffuse=float(fd),specularFloor=floor),uv=uv.tolist(),nativeUv=snapshots[1260]['r27'][:2].tolist(),halfError=float(np.max(abs(half-snapshots[1245]['r23'][:3]))),uvError=float(np.max(abs(uv-snapshots[1260]['r27'][:2]))),alphaError=float(abs(alpha-snapshots[1253]['r6'][3])),rampError=float(np.max(abs(ramp-snapshots[1261]['r26'][[0,2,3]]))),specularError=float(np.max(abs(spec-snapshots[1262]['r27'][:3]))),lobeError=float(abs(lobe-snapshots[1274]['r7'][1])),mainError=float(np.max(abs(main-snapshots[1279]['r11'][:3]))));rows.append(row)
for row in rows:
 assert row['halfError']<1e-5 and row['uvError']<1e-5 and row['alphaError']<1e-6,row
 assert row['rampError']<.001 and row['specularError']<.001 and row['lobeError']<1e-5 and row['mainError']<1e-5,row
report=dict(status='verified-variant-specular-intermediates',draws=rows,budgets=dict(half=1e-5,uv=1e-5,alpha=1e-6,ramp=.001,specular=.001,lobe=1e-5,main=1e-5),budgetSource='Existing cloth-specular-formula-proof verification; alpha additionally checks the observed packed roughness convention.',qualification='Debugger intermediate reconstruction; hardware and full material output require separate qualification.')
(run/'variant-specular-proof.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(rows,indent=2))
