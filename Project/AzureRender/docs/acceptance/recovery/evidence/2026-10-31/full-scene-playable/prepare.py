from pathlib import Path
import json,shutil,os,copy,hashlib
root=Path.cwd();r=Path(__file__).resolve().parent;source=root/'build/runs/20261031-172112-hair-source-inputs-14d199/project';target=r/'project';target.mkdir();binary_ext={'.glb','.bin','.dds','.png','.jpg','.jpeg','.azlut','.azirr','.azfog','.wav','.ogg','.fbx','.azprobe','.ktx2'};count=0
for p in sorted(source.rglob('*')):
 if not p.is_file() or '.azure' in p.relative_to(source).parts:continue
 q=target/p.relative_to(source);q.parent.mkdir(parents=True,exist_ok=True)
 if p.suffix.lower() in binary_ext:os.link(p,q)
 else:shutil.copy2(p,q)
 count+=1
old=root/'build/runs/20261027-002548-native-hair-layer-region-closure-61c105/main-candidate-v7';oldProject=json.loads((old/'playable-safe.azureproject').read_text());oldMounts={m['name']:old/m['path'] for m in oldProject['mounts']};oldScene=json.loads((oldMounts['candidate']/'playable-safe.azurelevel').read_text());scene=json.loads((source/'scene/full-restored.azurelevel').read_text());hero=next(n for n in scene['nodes'] if n['id']=='hero');oldHero=next(n for n in oldScene['nodes'] if n['id']=='hero')
for key in ['azure.animator','azure.character','azure.script','azure.transform']:hero['components'][key]=copy.deepcopy(oldHero['components'][key])
camera=next(n for n in scene['nodes'] if n['id']=='camera');oldCamera=next(n for n in oldScene['nodes'] if n['id']=='camera');camera['components']=copy.deepcopy(oldCamera['components']);camera['components']['azure.camera']['version']=2;camera['components']['azure.camera']['data'].update(nearPlane=.1,farPlane=10000,projectionOffsetX=0,projectionOffsetY=0)
if not any(n['id']=='native-railing-collision' for n in scene['nodes']):scene['nodes'].append(copy.deepcopy(next(n for n in oldScene['nodes'] if n['id']=='native-railing-collision')))
project=json.loads((source/'full-restored.azureproject').read_text());mounts={m['name']:target/m['path'] for m in project['mounts']};records=[];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['structural-collision.glb','native-railing-collision.glb','native-closed-speed-blend.json']:
 p=oldMounts['assets']/name;q=mounts['assets']/name;assert p.read_bytes()==q.read_bytes();records.append(dict(file=name,bytes=q.stat().st_size,sha256=sha(q)))
script=mounts['assets']/'atrium-safety.lua';oldText=script.read_text();assert 'candidate:/playable-safe.azurelevel' in oldText;script.write_text(oldText.replace('candidate:/playable-safe.azurelevel','stage:/playable.azurelevel'),encoding='utf-8');level=target/'scene/playable.azurelevel';level.write_text(json.dumps(scene,indent=2)+'\n');project['id']='complete-atrium-native-character-playable';project['name']='完整原生场景操控验证';project['startupScene']='stage:/playable.azurelevel';descriptor=target/'playable.azureproject';descriptor.write_text(json.dumps(project,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
report=dict(status='playable-candidate-prepared',baseCommit='e9d8a53',project=str(descriptor.relative_to(root)),copiedOrLinkedFiles=count,nodes=len(scene['nodes']),resources=len(scene['resources']),sourceInputs=records,characterModelSha256=sha(mounts['character']/'hero-restored-inputs-v8.glb'),projectSha256=sha(descriptor),levelSha256=sha(level),scriptSha256=sha(script),scope='Scene/component assembly only. Existing runtime, character material documents, full environment lighting and reflection settings are retained.');(r/'candidate.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
