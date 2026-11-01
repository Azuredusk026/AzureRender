import hashlib,json,os,shutil,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;prior=root/'build/runs/20261101-082926-joint-playable-display-c26c1b';source=prior/'project';target=run/'project';assert not target.exists();immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azirr','.azlut','.azfog','.hdr','.fbx','.wav','.ogg','.azprobe'};sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();original=json.loads((prior/'candidate.json').read_text())['inputs'];linked=copied=0
for directory,dirs,files in os.walk(source):
 dirs[:]=[d for d in dirs if d!='.azure']
 for name in files:
  p=Path(directory)/name;relative=p.relative_to(source);q=target/relative;q.parent.mkdir(parents=True,exist_ok=True)
  if p.suffix.lower() in immutable:os.link(p,q);linked+=1
  else:shutil.copy2(p,q);copied+=1
  assert sha(q)==original[relative.as_posix()]
for name in ['input-route.json','verify_behavior.py']:(run/name).write_bytes((prior/name).read_bytes())
(run/'capture_route.py').write_bytes((prior/'capture.py').read_bytes())
record=dict(status='prepared',project=str((target/'playable.azureproject').relative_to(root)),baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),inputs=original,frozenSourceFiles={str((source/name).relative_to(root)):digest for name,digest in original.items()},linkedFiles=linked,copiedFiles=copied)
(run/'candidate.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print('Prepared unchanged full project:',len(original),'input files')
