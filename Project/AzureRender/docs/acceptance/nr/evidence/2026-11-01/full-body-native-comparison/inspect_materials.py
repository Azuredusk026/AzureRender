import json
from pathlib import Path
run=Path(__file__).resolve().parent;rows=[]
for p in sorted((run/'project/assets/linear-display-materials').glob('*.azurematerial')):
 d=json.loads(p.read_text());m=d['surface']['materials'][0];s=m['extras']['azureRenderMaterial'];rows.append(dict(file=p.name,type=d['typeId'],name=m['name'],alphaMode=m.get('alphaMode'),surface=s))
(run/'material-inspection.json').write_bytes((json.dumps(rows,indent=2)+'\n').encode())
for r in rows:print(json.dumps({k:v for k,v in r.items() if k!='surfaceKeys'}))
