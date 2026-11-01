import copy,hashlib,json,os,shutil,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
source=root/'build/runs/20261101-043757-material-display-transform-93d6e0/project'
controls=root/'build/runs/20261031-183314-full-scene-playable-validation-d5257e/project'
target=run/'project';assert not target.exists()
immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azirr','.azlut','.azfog','.hdr','.fbx','.wav','.ogg','.azprobe'}
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
frozen={};linked=copied=0
for directory,dirs,files in os.walk(source):
 dirs[:]=[d for d in dirs if d!='.azure']
 for name in files:
  p=Path(directory)/name;relative=p.relative_to(source);q=target/relative;q.parent.mkdir(parents=True,exist_ok=True)
  frozen[str(p.relative_to(root))]=sha(p)
  if p.suffix.lower() in immutable:os.link(p,q);linked+=1
  else:shutil.copy2(p,q);copied+=1
scene=json.loads((source/'scene/full-world-linear.azurelevel').read_text());old=json.loads((controls/'scene/playable.azurelevel').read_text())
hero=next(n for n in scene['nodes'] if n['id']=='hero');oldhero=next(n for n in old['nodes'] if n['id']=='hero')
for name in ['azure.animator','azure.character','azure.script','azure.transform']:hero['components'][name]=copy.deepcopy(oldhero['components'][name])
camera=next(n for n in scene['nodes'] if n['id']=='camera');oldcamera=next(n for n in old['nodes'] if n['id']=='camera')
camera['components']=copy.deepcopy(oldcamera['components'])
scene['nodes'].append(copy.deepcopy(next(n for n in old['nodes'] if n['id']=='native-railing-collision')))
assert len({n['id'] for n in scene['nodes']})==len(scene['nodes'])
for name in ['structural-collision.glb','native-railing-collision.glb','native-closed-speed-blend.json']:
 assert (controls/'content-assets'/name).read_bytes()==(target/'content-assets'/name).read_bytes(),name
assert "stage:/playable.azurelevel" in (target/'content-assets/atrium-safety.lua').read_text()
rig=[n for n in scene['nodes'] if n.get('parentId')=='hero'];assert len(rig)==10
for n in rig:assert any(light['nodeId']==n['id'] and light['shape']['receivers']=={'schemaVersion':1,'surfaces':False,'characters':True} for light in scene['lights'])
descriptor=json.loads((source/'full-world-linear.azureproject').read_text());descriptor.update(id='joint-playable-linear-display',name='完整联合场景显示验证',startupScene='stage:/playable.azurelevel')
save=lambda p,d:p.write_bytes((json.dumps(d,indent=2,ensure_ascii=False)+'\n').encode())
save(target/'scene/playable.azurelevel',scene);save(target/'playable.azureproject',descriptor)
for p,digest in frozen.items():assert sha(root/p)==digest
inputs={p.relative_to(target).as_posix():sha(p) for p in target.rglob('*') if p.is_file()}
report=dict(status='prepared',baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),project=str((target/'playable.azureproject').relative_to(root)),sourceProject=str(source.relative_to(root)),controlProject=str(controls.relative_to(root)),nodes=len(scene['nodes']),resources=len(scene['resources']),linkedFiles=linked,copiedFiles=copied,characterLightChildren=[n['id'] for n in rig],inputs=inputs,frozenSourceFiles=frozen)
save(run/'candidate.json',report);(run/'.keep').write_text('Joint playable display candidate. Retain until formal acceptance.\n')
for name in ['input-route.json','verify_behavior.py']:shutil.copy2(controls.parent/name,run/name)
print(json.dumps({k:v for k,v in report.items() if k not in {'inputs','frozenSourceFiles'}}))
