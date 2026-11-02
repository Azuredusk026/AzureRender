import json,struct
from pathlib import Path
run=Path(__file__).resolve().parent;project=run/'candidate-project';definition=json.loads((project/'full-world-hidden.azureproject').read_text());mounts={m['name']:project/m['path'] for m in definition['mounts']};scene=json.loads((project/'scene/full-world-hidden.azurelevel').read_text());seen=set();offset=0;rows=[];target=693;identities={}
for folder in mounts.values():
 for meta in folder.rglob('*.azmeta'):
  identity=json.loads(meta.read_text()).get('id')
  if identity:identities[identity]=meta.with_suffix('')
for resource in scene['resources']:
 ref=resource['asset']
 if ref in identities:path=identities[ref].resolve()
 elif ':/' in ref:
  mount,relative=ref.split(':/',1);path=(mounts[mount]/relative).resolve()
 else:path=(project/ref).resolve()
 if path in seen:continue
 seen.add(path)
 with path.open('rb') as stream:
  header=stream.read(20)
  if header[:4]==b'glTF':n=struct.unpack_from('<I',header,12)[0];gltf=json.loads(stream.read(n))
  else:stream.seek(0);gltf=json.load(stream)
 materials=gltf.get('materials',[])
 if offset<=target<offset+len(materials):
  local=target-offset;material=materials[local];profile=material.get('extras',{}).get('azureRenderMaterial',{});primitives=[dict(mesh=m.get('name'),indices=gltf['accessors'][p['indices']]['count'],vertices=gltf['accessors'][p['attributes']['POSITION']]['count']) for m in gltf['meshes'] for p in m['primitives'] if p.get('material',0)==local];row=dict(globalIndex=target,localIndex=local,resourceId=resource['id'],asset=str(path),material=material,primitives=primitives);rows.append(row);print(material['name'],material.get('alphaMode','OPAQUE'),str(path),primitives);print('PROFILE',json.dumps(profile))
 # The production loader appends one fallback material to every asset.
 offset+=len(materials)+1
 if len(seen)==1:offset+=1  # Primary asset owns the showcase-platform material.
(run/'pixel-material-693-mapping.json').write_bytes((json.dumps(dict(status='mapped-raw-resource-order',materialCount=offset,matches=rows),indent=2)+'\n').encode());print('Material count',offset)
