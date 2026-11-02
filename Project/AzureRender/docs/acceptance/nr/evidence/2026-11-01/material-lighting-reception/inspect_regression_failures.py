import json
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;rows=[]
p=run/'related-gpu/release/object-shadow/shadow/diagnostics/azureplayer.log.jsonl'
logs=[json.loads(line) for line in p.read_text().splitlines()]
bad=[r for r in logs if r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning']
print('LOG records',len(logs),'errors/validation warnings',len(bad),'timestamps',logs[0]['timestamp_ms'],logs[-1]['timestamp_ms'])
for row in bad[-2:]:print('LOG',row['timestamp_ms'],row['level'],row['subsystem'],str(row['message'])[:260])
old=root/'build/runs/20261101-043757-material-display-transform-93d6e0/native-final-captures/full-world-hidden/frames';new=run/'native-captures/full-world-hidden/frames'
for i in range(12):
    name=f'frame_{i:06d}.png';a=np.asarray(Image.open(old/name).convert('RGB'));b=np.asarray(Image.open(new/name).convert('RGB'));d=np.abs(a.astype(np.int16)-b.astype(np.int16));mask=np.any(d!=0,axis=2);y,x=np.where(mask);row=dict(frame=i,differentPixels=len(x),maximumByteError=int(d.max()),bounds=[int(x.min()),int(y.min()),int(x.max())+1,int(y.max())+1] if len(x) else None)
    if len(x)<=24:row['pixels']=[dict(x=int(xx),y=int(yy),before=a[yy,xx].tolist(),after=b[yy,xx].tolist()) for xx,yy in zip(x,y)]
    rows.append(row);print('IMAGE',{k:v for k,v in row.items() if k!='pixels'})
(run/'compatibility-red-diagnostics.json').write_bytes((json.dumps(rows,indent=2)+'\n').encode())
