import json,struct
from pathlib import Path
run=Path(__file__).resolve().parent;m=json.loads((run/'rdc-parallel-frame4-layer/target/frames/capture_manifest.json').read_text());print('PRIMARY',[(v['index'],v['name'],v['class']) for v in m['materialInventory']]);print('HERO',[(v['materialIndex'],v['name']) for v in m['materialLightingReceivers']]);p=Path(m['asset'])
with p.open('rb') as f:h=f.read(20);d=json.loads(f.read(struct.unpack_from('<I',h,12)[0]))
print('RAW PRIMARY',[x['name'] for x in d['materials']]);s=json.loads((run/'candidate-project/scene/full-world-hidden.azurelevel').read_text());print('FIRST RESOURCES',[(r['id'],r['asset']) for r in s['resources'][:2]])
