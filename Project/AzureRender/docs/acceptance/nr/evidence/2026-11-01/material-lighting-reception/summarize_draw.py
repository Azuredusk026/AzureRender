import json
from pathlib import Path
run=Path(__file__).resolve().parent;d=json.loads((run/'pixel-draw-details/pipelines.json').read_text())[0]
def visit(items):
 for v in items:
  if any(k in v['name'].lower() for k in ['material','feature','opacity','texturebase']):print(v['name'],v['float32'],v['uint32'])
  visit(v['members'])
for stage in d['stages']:
 print('STAGE',stage['stage'])
 for block in stage['constant_blocks']:
  print('BLOCK',block['name'],block['length'],[v['name'] for v in block['variables']]);visit(block['variables'])
print('ACTION',d['action'])
