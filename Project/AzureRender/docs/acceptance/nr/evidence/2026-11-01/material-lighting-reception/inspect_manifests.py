import json
from pathlib import Path
run=Path(__file__).resolve().parent;prior=Path(json.loads((run/'candidate.json').read_text())['referenceRun']);paths={'source':prior/'captures/fullbody-source-aa0/frames/capture_manifest.json','control':run/'source-environment-control-no-ssr/frames/capture_manifest.json'};rows={}
for name,path in paths.items():
 d=json.loads(path.read_text());row={k:d[k] for k in ['materialClassFeatureSpecializations','toonRampProfile','toonRampProfileHash','toonRampAtlasHash','qaShowcasePreset','stylizedLighting','irradianceResourceKind','irradianceQueryScope','gradeSettings','qaCameraPosition','qaCameraTarget','cameraLens','shadowSettings','materialInventory']};rows[name]=row
 for key,value in row.items():
  if key=='materialInventory':
   print(name,key,'type',type(value).__name__,'length',len(value));print(str(value)[:650])
  else:print(name,key,str(value)[:1300])
(run/'manifest-inspection.json').write_bytes((json.dumps(rows,indent=2)+'\n').encode())
