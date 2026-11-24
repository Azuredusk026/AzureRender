import copy,hashlib,json,os,shutil,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;prior=root/'build/runs/20261101-082926-joint-playable-display-c26c1b';source=prior/'project';target=run/'project';assert not target.exists()
immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azirr','.azlut','.azfog','.hdr','.fbx','.wav','.ogg','.azprobe'};sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();frozen={}
for directory,dirs,files in os.walk(source):
 dirs[:]=[d for d in dirs if d!='.azure']
 for name in files:
  p=Path(directory)/name;q=target/p.relative_to(source);q.parent.mkdir(parents=True,exist_ok=True);frozen[p.relative_to(root).as_posix()]=sha(p)
  if p.suffix.lower() in immutable:os.link(p,q)
  else:shutil.copy2(p,q)
base=json.loads((source/'scene/near-world-linear.azurelevel').read_text());old=json.loads((prior/'standalone-project/scene/source-standalone.azurelevel').read_text());descriptor=json.loads((source/'near-world-linear.azureproject').read_text());byid={n['id']:n for n in base['nodes']};cases=[]
save=lambda p,d:p.write_bytes((json.dumps(d,ensure_ascii=False,indent=2)+'\n').encode())
for aa in [0,3]:
 for kind in ['world','source']:
  level=copy.deepcopy(base);camera=next(n for n in level['nodes'] if n['id']=='camera');camera['components']['azure.transform']['data'].update(translation=[-1.5,2.12,-20],rotation=[0,180,0]);camera['components']['azure.camera']['data'].update(fieldOfView=32,projectionOffsetX=0,projectionOffsetY=0)
  if kind=='source':
   level['renderSettings']=copy.deepcopy(old['renderSettings']);level['lights']=copy.deepcopy(old['lights']);wanted={'hero','camera'}|{l['nodeId'] for l in level['lights']}|{n['id'] for n in base['nodes'] if n.get('parentId')=='hero'}
   while True:
    ancestors={byid[n].get('parentId','') for n in wanted};ancestors.discard('');expanded=wanted|ancestors
    if expanded==wanted:break
    wanted=expanded
   level['nodes']=[n for n in level['nodes'] if n['id'] in wanted];level['resources']=[r for r in level['resources'] if r['id']=='hero'];level['sceneImports']={}
   for n in level['nodes']:
    if n['id']!='hero':
     n['resourceId']=''
     if 'azure.renderable' in n['components']:n['components']['azure.renderable']['data']['visible']=False
   for key in ['irradianceVolume','reflectionProbe','viewFog']:level.pop(key,None)
  level['renderSettings'].update(antiAliasing=aa,schemaVersion=max(18,level['renderSettings'].get('schemaVersion',1)))
  name=f'fullbody-{kind}-aa{aa}';level['id']=name;desc=copy.deepcopy(descriptor);desc.update(id=name,name=name,startupScene='stage:/'+name+'.azurelevel');save(target/(name+'.azureproject'),desc);save(target/'scene'/(name+'.azurelevel'),level)
  cases.append(dict(name=name,kind=kind,aa=aa,frames=64,width=1536,height=1536,nodes=len(level['nodes']),resources=len(level['resources']),project=str(target/(name+'.azureproject'))))
inputs={p.relative_to(target).as_posix():sha(p) for p in target.rglob('*') if p.is_file()};build=json.loads((root/'build/runs/20261101-101614-view-fog-reuse-814a54/release-build.json').read_text());assert sha(root/'build/ninja-msvc-release/AzurePlayer.exe')==build['programs']['AzurePlayer'];save(run/'candidate.json',dict(status='prepared',baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),cases=cases,inputs=inputs,frozenSourceFiles=frozen,programSha256=build['programs']['AzurePlayer'],shaders=build['shaders'],visualAcceptance='failed-user-review'));(run/'.keep').write_text('Full-body native comparison. Retain through visual acceptance.\n');print(json.dumps(cases))
