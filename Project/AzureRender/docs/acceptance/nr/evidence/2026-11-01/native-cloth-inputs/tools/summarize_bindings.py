import hashlib,json
from pathlib import Path
import numpy as np
run=Path(__file__).resolve().parent;report=json.loads((run/'native-bindings.json').read_text());assert report['status']=='exported-native-bindings';rows=[]
for draw in report['draws']:
 for file in [draw['shader'],*[b for b in draw['constantBuffers']],*[t['export'] for t in draw['textures'] if 'export' in t]]:
  p=run/'native-bindings'/file['path'];assert p.stat().st_size==file['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==file['sha256']
 cb6=next(b for b in draw['constantBuffers'] if b['index']==6);values=np.fromfile(run/'native-bindings'/cb6['path'],dtype='<f4').reshape(-1,4);ramp=next(t for t in draw['textures'] if t['slot']==12)
 rows.append(dict(event=draw['event'],shaderSha256=draw['shader']['sha256'],specularRampResource=ramp['resource'],specularRampSha256=ramp['export']['sha256'],materialConstantBytes=cb6['size'],firstMaterialRows=values[:3].tolist()))
result=dict(status='measured-resource-bindings',draws=rows,sourceCaptureSha256=report['captureSha256']);(run/'native-binding-measurement.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
for row in rows:print(row['event'],row['shaderSha256'][:12],row['specularRampResource'],row['specularRampSha256'],row['materialConstantBytes'])
assert len(rows)==4 and rows[1]['shaderSha256']==rows[2]['shaderSha256']
