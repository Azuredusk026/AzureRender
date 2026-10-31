from pathlib import Path
import json,os,shutil,hashlib,collections
root=Path.cwd();r=(root/'build/runs/20261031-164808-character-frontal-lighting-fa3879').resolve();source=root/'build/runs/20261031-105539-full-scene-character-composition-b37668/project';target=r/'project';descriptor=json.loads((source/'full-character-visible.azureproject').read_text());records=[];binary={'.glb','.bin','.dds','.png','.jpg','.jpeg','.azlut','.azirr','.azfog','.wav','.ogg','.fbx','.azprobe','.ktx2'}
for mount in descriptor['mounts']:
 if mount['name']=='stage':continue
 folder=source/mount['path'];destination=target/mount['path'];assert destination.resolve().is_relative_to(target.resolve())
 for p in sorted(folder.rglob('*')):
  if not p.is_file() or '.azure' in p.relative_to(folder).parts:continue
  q=destination/p.relative_to(folder);q.parent.mkdir(parents=True,exist_ok=True);assert not q.exists()
  if p.suffix.lower() in binary:os.link(p,q);mode='binary-hardlink'
  else:shutil.copy2(p,q);mode='independent-copy'
  records.append(dict(path=q.relative_to(target).as_posix(),bytes=q.stat().st_size,mode=mode))
for name in ['control','candidate']:
 p=target/(name+'.azureproject');document=json.loads(p.read_text())
 for mount in document['mounts']:mount['path']=next(x['path'] for x in descriptor['mounts'] if x['name']==mount['name'])
 p.write_text(json.dumps(document,indent=2)+'\n')
failed=r/'control-beauty';destination=r/'failed-external-mount';assert failed.resolve().is_relative_to(r) and destination.resolve().is_relative_to(r) and not destination.exists();failed.rename(destination);shutil.copyfile(r/'summary.json',destination/'summary.json');shutil.copyfile(r/'capture.log',destination/'capture.log');(r/'asset-copy.json').write_text(json.dumps(dict(status='project-contained-inputs-prepared',records=records),indent=2)+'\n');print(collections.Counter(x['mode'] for x in records),len(records),'project-contained files')
