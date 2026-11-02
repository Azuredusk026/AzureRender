import json
from pathlib import Path
run=Path(__file__).resolve().parent;d=json.loads((run/'rdc-parallel-frame4-layer/target/frames/capture_manifest.json').read_text())
print('KEYS',list(d))
for key in ['sceneImports','sceneResources','resources','materialNames','materialProfiles']:
 if key not in d:continue
 v=d[key];print(key,type(v).__name__,len(v) if isinstance(v,(dict,list)) else '')
 if isinstance(v,list) and v:print('FIRST',list(v[0]) if isinstance(v[0],dict) else str(v[0])[:160])
 elif isinstance(v,dict):print('FIELDS',list(v))
