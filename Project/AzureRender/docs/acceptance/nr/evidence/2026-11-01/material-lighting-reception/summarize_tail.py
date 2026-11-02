import json
from pathlib import Path
run=Path(__file__).resolve().parent;d=json.loads((run/'parallel-tail-inspection.json').read_text());print(d['status'],d.get('drawCount'),d.get('temporalEvent'),d.get('error',''))
for row in d['draws']:
 print(row['event'],row['program'],len(row['targets']),row['indices'])
 if row.get('event')==d.get('temporalEvent'):
  print('TEMPORAL TARGETS',json.dumps(row['targets']));print('TEMPORAL INPUTS',json.dumps(row['inputs']))
