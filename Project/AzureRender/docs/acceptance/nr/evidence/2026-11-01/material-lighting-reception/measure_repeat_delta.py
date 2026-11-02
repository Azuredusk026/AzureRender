import json
from pathlib import Path
import numpy as np
from PIL import Image
run=Path(__file__).resolve().parent;rows=[]
for i in range(12):
 name=f'frame_{i:06d}.png';a=np.asarray(Image.open(run/'native-captures/full-world-hidden/frames'/name).convert('RGB'));b=np.asarray(Image.open(run/'compatibility-repeat/full-world-hidden/frames'/name).convert('RGB'));delta=np.abs(a.astype(np.int16)-b.astype(np.int16));mask=np.any(delta!=0,axis=2);y,x=np.where(mask)
 row=dict(frame=i,differentPixels=len(x),maximumByteError=int(delta.max()),bounds=[int(x.min()),int(y.min()),int(x.max())+1,int(y.max())+1] if len(x) else None)
 if len(x)<=32:row['pixels']=[dict(x=int(xx),y=int(yy),before=a[yy,xx].tolist(),after=b[yy,xx].tolist()) for xx,yy in zip(x,y)]
 rows.append(row)
(run/'repeat-delta-diagnostics.json').write_bytes((json.dumps(dict(status='measured',sameProgramAndInputs=True,cases=rows),indent=2)+'\n').encode())
print(json.dumps([{k:v for k,v in row.items() if k!='pixels'} for row in rows if row['differentPixels']]))
