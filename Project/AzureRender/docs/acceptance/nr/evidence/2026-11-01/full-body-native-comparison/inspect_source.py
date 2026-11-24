import json
from pathlib import Path
run=Path(__file__).resolve().parent;rows=[]
for i in [1,2,3,4,5]:
 p=run/f'project/assets/linear-display-materials/material-{i}.azurematerial';m=json.loads(p.read_text())['surface']['materials'][0];extras=m['extras'];source=extras['nativeCharacterProfileSource'];rows.append(dict(index=i,name=m['name'],profile=extras['azureRenderMaterial'],sourceKeys=list(source),floats={k:v for k,v in source.get('floats',{}).items() if any(s in k.lower() for s in ['light','shadow','gi','env','spec','ramp','albedo','diffuse','blend','surface','metal','rough'])},colors=source.get('colors',{})))
(run/'source-inspection.json').write_bytes((json.dumps(rows,indent=2)+'\n').encode())
for row in rows:print(json.dumps({k:v for k,v in row.items() if k not in {'profile','colors'}}))
