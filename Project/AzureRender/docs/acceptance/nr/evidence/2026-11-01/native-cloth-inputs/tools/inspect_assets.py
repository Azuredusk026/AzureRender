import json,struct
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;source=root/'build/runs/20261101-120125-character-environment-reception-fa70a7/candidate-project';paths=list(source.rglob('hero-restored-inputs-v8.glb'));assert len(paths)==1;data=paths[0].read_bytes();gltf=json.loads(data[20:20+struct.unpack_from('<I',data,12)[0]])
images=[dict(index=i,name=v.get('name'),storage='uri' if 'uri' in v else 'bufferView',bufferView=v.get('bufferView')) for i,v in enumerate(gltf['images'])]
profiles=[dict(index=i,name=v['name'],profile=v.get('extras',{}).get('azureRenderMaterial')) for i,v in enumerate(gltf['materials']) if i in [1,2,3,4,5]]
(run/'asset-image-index.json').write_bytes((json.dumps(dict(images=images,profiles=profiles),indent=2)+'\n').encode())
print('images',len(images),'textures',len(gltf['textures']))
for v in images:
 if 'ramp' in (v['name'] or '').lower():print(v)
for p in profiles:print('material',p['index'],'specularRamp',p['profile'].get('specularRamp'),'nprDiffuse',p['profile'].get('nprDiffuse',{}).get('shadowAlbedo'))
