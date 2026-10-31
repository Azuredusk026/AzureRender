import json,struct
from pathlib import Path
import numpy as np
run=Path(__file__).resolve().parent;path=run/'inverse-feasibility.json';data=json.loads(path.read_text())
raw=Path(data['inputs']['lut']).read_bytes();size=struct.unpack_from('<I',raw,8)[0];vertices=np.frombuffer(raw,dtype='<f4',offset=12+size).reshape(-1,4)[:,:3].astype(float)
desired=np.array([x['displayRgb'] for x in data['samples']]);attained=np.array([x['attainedRgb'] for x in data['samples']]);delta=desired-attained
norm=np.linalg.norm(delta,axis=1);directions=delta[norm>1e-5]/norm[norm>1e-5,None]
contrast=np.array([[1,-1,0],[1,0,-1],[0,1,-1],[-1,1,0],[-1,0,1],[0,-1,1]],dtype=float)/np.sqrt(2)
directions=np.vstack([contrast,directions]);certificates=[]
for normal in directions:
    support=float(np.max(vertices@normal));separation=desired@normal-support
    for index in np.flatnonzero(separation>1e-6):
        certificates.append({'sample':int(index),'region':data['samples'][index]['region'],'targetRgb':desired[index].tolist(),
            'unitPlaneNormal':normal.tolist(),'maximumLutVertexProjection':support,'targetProjection':float(desired[index]@normal),
            'separation':float(separation[index]),'minimumMaximumChannelError':float(separation[index]/np.sum(np.abs(normal)))})
certificates.sort(key=lambda x:x['minimumMaximumChannelError'],reverse=True)
result={'status':'measured','basis':'Any trilinear LUT sample is a convex combination of lattice values. A target exceeding a lattice support plane is unattainable for every LUT coordinate. The L1 norm of the plane normal converts the separation into a lower bound on maximum channel error.',
    'certifiedUnattainableSamples':len({x['sample'] for x in certificates}),'maximumCertifiedChannelErrorLowerBound':max((x['minimumMaximumChannelError'] for x in certificates),default=0),
    'certificates':certificates}
(run/'gamut-certificate.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({key:value for key,value in result.items() if key!='certificates'}))
