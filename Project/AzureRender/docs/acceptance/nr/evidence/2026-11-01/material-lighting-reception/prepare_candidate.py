import hashlib,json,os,shutil
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;source=run/'project';target=run/'candidate-project';assert not target.exists();immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azirr','.azlut','.azfog','.hdr','.fbx','.wav','.ogg','.azprobe'};sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();frozen={}
for directory,dirs,files in os.walk(source):
 dirs[:]=[d for d in dirs if d!='.azure']
 for name in files:
  p=Path(directory)/name;q=target/p.relative_to(source);q.parent.mkdir(parents=True,exist_ok=True);frozen[p.relative_to(source).as_posix()]=sha(p)
  if p.suffix.lower() in immutable:os.link(p,q)
  else:shutil.copy2(p,q)
base=json.loads((target/'scene/fullbody-world-aa0.azurelevel').read_text());hero=next(n for n in base['nodes'] if n['id']=='hero');materials=[]
for i in [1,2,3,4,5]:
 path=target/f'assets/linear-display-materials/material-{i}.azurematerial';before=sha(path);document=json.loads(path.read_text());material=document['surface']['materials'][0];profile=material['extras']['azureRenderMaterial'];identity=json.loads(path.with_suffix(path.suffix+'.azmeta').read_text())['id'];assert hero['materialSlots']['name:'+material['name']]==identity;assert profile['class']=='fabric' and profile['radianceScale']==.25 and profile['displayTransform']=='linear';profile['environmentLighting']=dict(schemaVersion=1,diffuse='global-environment',reflection='global-environment',globalIntensity=.5)
 if i in [2,3,5]:
  assert 'nprDiffuse' not in profile;profile['mainLightShadow']='object-shadow'
 else:assert 'nprDiffuse' in profile
 path.write_bytes((json.dumps(document,ensure_ascii=False,indent=2)+'\n').encode());materials.append(dict(index=i,path=path.relative_to(target).as_posix(),id=identity,name=material['name'],beforeSha256=before,afterSha256=sha(path),mainLightShadow=profile.get('mainLightShadow','scene')))
for name,digest in frozen.items():assert sha(source/name)==digest,name
record=dict(status='prepared',materials=materials,sourceProject=str(source),sourceInputs=frozen,inputs={p.relative_to(target).as_posix():sha(p) for p in target.rglob('*') if p.is_file()},projects={name:str(target/(name+'.azureproject')) for name in ['fullbody-world-aa0','fullbody-source-aa0','fullbody-world-aa3','fullbody-source-aa3','near-world-linear','full-world-hidden','full-world-linear','playable']},visualAcceptance='failed-user-review');(run/'lighting-candidate.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print('Prepared five fabric environment policies and three ordinary object-shadow receivers')
