import hashlib,json,struct
from pathlib import Path
import numpy as np
root=Path.cwd();run=Path(__file__).resolve().parent
path=root/'build/runs/20261031-231334-receiver-directional-light-911ab7/project/native-gi/captured-mirrored.azic'
raw=path.read_bytes();assert raw[:8]==b'AZIC0001';dimensions=struct.unpack_from('<3I',raw,8)
rows=np.array(struct.unpack_from('<48f',raw,24)).reshape(12,4);transform=rows[:3];layers=rows[6:].reshape(3,2,4)
points={'native-camera':[-.266043,2.262059,-24.905325],'fixed-character-feet':[-1.5,1.035,-16],'fixed-character-head':[-1.5,2.65,-16], 'playable-character-feet':[0,1.035,9],'playable-character-head':[0,2.65,9], 'central-floor':[0,1.035,-10], 'front-floor':[0,1.035,-20]}
cases=[]
for name,world in points.items():
    p=transform@np.r_[world,1];fades=[]
    for center,fade in layers:
        d=np.abs(p-center[:3]);fades.append(float(np.clip(max((max(d[0],d[2])-fade[0])/fade[2],(d[1]-fade[1])/fade[2]),0,1)))
    f=np.array(fades);weights=[1-f[0],f[0]*(1-f[1]),f[1]*(1-f[2])]
    cases.append({'name':name,'world':world,'fieldPosition':p.tolist(),'fades':fades,'weights':weights,'outsideAllLayers':bool(f[2]>=1)})
result={'source':str(path.relative_to(root)),'sha256':hashlib.sha256(raw).hexdigest(),'dimensions':dimensions,'worldToField':transform.tolist(),'defaultCoefficients':rows[3:6].tolist(),'layers':layers.tolist(),'scope':'Actual static clipmap coverage at fixed and playable receiver points; camera position does not rebase authored layers. This is coverage only, not final radiance or image acceptance.','cases':cases}
(run/'gi-coverage.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
