from pathlib import Path
import json,hashlib
import numpy as np
from PIL import Image
r=Path(__file__).resolve().parent;root=Path.cwd();old=root/'build/runs/20261024-074605-native-material-aa-reference-9eb92d';current=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference';regions=[('image-left-eye',[620,124,638,139]),('image-right-eye',[646,124,664,139])];report=dict(status='measured',scope='Fixed image-space rectangles selected from the original 1280x720 portrait. Rectangles include eyelids and skin; they are not native visibility masks or final acceptance gates.',cases=[])
for kind,index,scale in [('high',0,4),('temporal',31,1)]:
 a=old/kind/'frames'/('frame_%06d.png'%index);b=current/kind/'frames'/a.name;x=np.asarray(Image.open(a).convert('RGB'));y=np.asarray(Image.open(b).convert('RGB'));row=dict(case=kind,reference=dict(path=str(a.relative_to(root)),sha256=hashlib.sha256(a.read_bytes()).hexdigest()),current=dict(path=str(b.relative_to(root)),sha256=hashlib.sha256(b.read_bytes()).hexdigest()),regions=[])
 for name,box in regions:
  x0,y0,x1,y1=[v*scale for v in box];d=np.abs(x[y0:y1,x0:x1].astype('i2')-y[y0:y1,x0:x1].astype('i2'));row['regions'].append(dict(name=name,boxAt1280x720=box,scale=scale,pixels=d.shape[0]*d.shape[1],changedPixels=int(np.any(d>0,axis=2).sum()),pixelsBeyondOneByte=int(np.any(d>1,axis=2).sum()),meanByteError=float(d.mean()),maximumByteDifference=int(d.max())))
 report['cases'].append(row)
(r/'eye-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
