import gzip,json
from pathlib import Path
import numpy as np
run=Path(__file__).resolve().parent;report=json.loads((run/'recording-pair-inputs.json').read_text());assert report['status']=='measured-recording-pair-inputs';a,b=report['captures'];assert [v['kind'] for v in [a,b]]==['parallel','serial'];differences=[]
def compare(x,y,path=''):
 if type(x)!=type(y):differences.append(dict(path=path,left=x,right=y));return
 if isinstance(x,dict):
  assert set(x)==set(y),(path,set(x)^set(y))
  for key in x:compare(x[key],y[key],path+'/'+key)
 elif isinstance(x,list):
  assert len(x)==len(y),path
  for i,(u,v) in enumerate(zip(x,y)):compare(u,v,path+'/'+str(i))
 elif x!=y:differences.append(dict(path=path,left=x,right=y))
compare(a['constants'],b['constants']);files=[]
for group in ['buffers','textures']:
 left={v['name']:v for v in a[group]};right={v['name']:v for v in b[group]};assert set(left)==set(right)
 for name,x in left.items():
  y=right[name];entry=dict(group=group,name=name,identical=x['sha256']==y['sha256'],bytes=[x['bytes'],y['bytes']]);files.append(entry)
  if not entry['identical']:
   data=[np.frombuffer(gzip.decompress((run/'recording-pair-inputs'/v['path']).read_bytes()),dtype=np.uint8) for v in [x,y]];assert len(data[0])==len(data[1]);indices=np.flatnonzero(data[0]!=data[1]);entry.update(differentBytes=len(indices),firstOffsets=indices[:24].tolist())
   if group=='textures' and x['format']=='R32_FLOAT':
    floats=[d.view('<f4').reshape(x['height'],x['width']) for d in data];delta=abs(floats[0]-floats[1]);ys,xs=np.where(delta!=0);entry.update(differentPixels=len(xs),maximumFloatError=float(delta.max()),bounds=[int(xs.min()),int(ys.min()),int(xs.max()),int(ys.max())] if len(xs) else None)
hdr=[]
for row in [a,b]:hdr.append(np.frombuffer(gzip.decompress((run/'recording-pair-inputs'/row['mainHdr']['path']).read_bytes()),dtype='<f2').reshape(1440,2560,4))
delta=abs(hdr[0].astype(np.float32)-hdr[1].astype(np.float32));ys,xs=np.where(np.any(delta!=0,axis=2));result=dict(status='measured-recording-input-differences',events=[a['event'],b['event']],constantDifferences=differences,resources=files,mainHdr=dict(differentPixels=len(xs),maximumFloatError=float(np.nanmax(delta)),bounds=[int(xs.min()),int(ys.min()),int(xs.max()),int(ys.max())] if len(xs) else None,pixel911x4=[v['pixel911x4'] for v in [a,b]]),visualAcceptance='failed-user-review')
(run/'recording-pair-comparison.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print(json.dumps(dict(constants=differences,differentResources=[v for v in files if not v['identical']],mainHdr=result['mainHdr']),indent=2))
