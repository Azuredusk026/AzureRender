import copy,hashlib,json,os,shutil
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;source=run/'project';target=run/'standalone-project';assert not target.exists()
immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azirr','.azlut','.azfog','.hdr','.fbx','.wav','.ogg','.azprobe'}
for directory,dirs,files in os.walk(source):
 dirs[:]=[d for d in dirs if d!='.azure']
 for name in files:
  p=Path(directory)/name;q=target/p.relative_to(source);q.parent.mkdir(parents=True,exist_ok=True)
  if p.suffix.lower() in immutable:os.link(p,q)
  else:shutil.copy2(p,q)
base=json.loads((source/'scene/near-world-linear.azurelevel').read_text());descriptor=json.loads((source/'near-world-linear.azureproject').read_text())
original=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/high/reference.azurelevel'
old=json.loads(original.read_text());byid={n['id']:n for n in base['nodes']};save=lambda p,d:p.write_bytes((json.dumps(d,indent=2,ensure_ascii=False)+'\n').encode());cases=[]
for name,original_style in [('geometry-only',False),('source-standalone',True)]:
 level=copy.deepcopy(base);lights=[light for light in level['lights'] if not original_style or light['shape']['receivers']['characters']]
 wanted={'hero','camera'}|{light['nodeId'] for light in lights}|{n['id'] for n in base['nodes'] if n.get('parentId')=='hero'}
 while True:
  ancestors={byid[name].get('parentId','') for name in wanted};ancestors.discard('');updated=wanted|ancestors
  if updated==wanted:break
  wanted=updated
 level['nodes']=[n for n in level['nodes'] if n['id'] in wanted]
 for node in level['nodes']:
  if node['id']!='hero':
   node['resourceId']=''
   if 'azure.renderable' in node['components']:node['components']['azure.renderable']['data']['visible']=False
 level['resources']=[r for r in level['resources'] if r['id']=='hero'];assert len(level['resources'])==1
 level['sceneImports']={};level['lights']=lights
 if original_style:
  level['renderSettings']=copy.deepcopy(old['renderSettings']);settings=level['renderSettings'];settings.update(antiAliasing=0,exposure=0,toneMapping=False,bloomEnabled=False);settings.pop('gradeLut',None)
  if 'screenSpaceReflection' in settings:settings['screenSpaceReflection']['enabled']=False
  level.pop('irradianceVolume',None);level.pop('reflectionProbe',None);level.pop('viewFog',None)
  for light in level['lights']:light['shape']['receivers']={'schemaVersion':1,'surfaces':True,'characters':True}
  assert len(level['lights'])==10
 level['id']='native-character-'+name;desc=copy.deepcopy(descriptor);desc.update(id='native-character-'+name,name='native-character-'+name,startupScene='stage:/'+name+'.azurelevel')
 path=target/(name+'.azureproject');scene_path=target/'scene'/(name+'.azurelevel');save(path,desc);save(scene_path,level)
 cases.append(dict(name=name,project=str(path),level=str(scene_path),nodes=len(level['nodes']),resources=1,lights=len(level['lights']),originalStyle=original_style))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();fingerprints={p.relative_to(target).as_posix():sha(p) for p in target.rglob('*') if p.is_file()}
save(run/'standalone-inputs.json',dict(status='prepared',sourceOriginal=str(original),sourceOriginalSha256=sha(original),cases=cases,inputs=fingerprints));print(json.dumps(cases))
