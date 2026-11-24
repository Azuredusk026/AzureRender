import hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;source=root/'build/runs/20261017-000956-endfield-native-3caf73/laevat-static';audit=json.loads((source/'source_audit.json').read_text());artifacts=json.loads((source/'source_texture_artifacts.json').read_text());rows=[];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for material in audit['materials']:
 if material['object']['name'] not in ['M_actor_laevat_cloth_0'+str(i) for i in [1,2,3,4,5]]:continue
 textures=[]
 for binding in material['textures']:
  if binding['slot'] not in ['_SpecRampMap','_DiffRampMap','_MetallicGlossMap']:continue
  identity=binding['reference'].get('resolved')
  if not identity:continue
  artifact=next(a for a in artifacts if a['object']['source']==identity['source'] and a['object']['path_id']==identity['path_id']);path=source/artifact['png']['path'];assert path.is_file() and sha(path)==artifact['png']['sha256'].lower()
  textures.append(dict(slot=binding['slot'],source=identity,colorSpace=binding['color_space'],scale=binding['scale'],offset=binding['offset'],pngPath=str(path.relative_to(root)),sha256=sha(path)))
 rows.append(dict(name=material['object']['name'],source=material['object'],shader=material.get('shader'),textures=textures))
report=dict(status='authored-source-candidates-verified',qualification='Authored material source; actual captured shader resource bindings remain independently required.',sourceFiles={name:sha(source/name) for name in ['source_audit.json','source_texture_artifacts.json']},materials=rows)
(run/'authored-texture-sources.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
for row in rows:print(row['name'],[(v['slot'],v['source']['name'],v['sha256'][:12]) for v in row['textures']])
