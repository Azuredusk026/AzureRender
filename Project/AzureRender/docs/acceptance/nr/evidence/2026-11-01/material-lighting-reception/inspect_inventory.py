import json
from pathlib import Path
run=Path(__file__).resolve().parent;d=json.loads((run/'rdc-parallel-frame4-layer/target/frames/capture_manifest.json').read_text());v=d['materialInventory'];print(type(v).__name__,len(v));print(list(v)[:10] if isinstance(v,dict) else v[:1]);
if isinstance(v,list) and len(v)>703:print('MATERIAL703',v[703])
elif isinstance(v,dict):
 for k,x in v.items():
  if isinstance(x,list):print(k,len(x),x[703] if len(x)>703 else x[:1])
