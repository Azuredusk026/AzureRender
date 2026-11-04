from pathlib import Path
import hashlib,json,os,shutil
root=Path.cwd();r=Path(__file__).resolve().parent;source=r.parent/'qualified-lit-surfaces-playable';target=r/'source-project';assert not target.exists()
def copy(a,b):
 p=Path(a);q=Path(b)
 if p.suffix.lower() in {'.azureproject','.azurelevel','.azurematerial','.json','.lua','.azmeta','.md','.txt'}:shutil.copy2(p,q)
 else:os.link(p,q)
 return str(q)
shutil.copytree(source,target,ignore=shutil.ignore_patterns('.azure','__pycache__'),copy_function=copy)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();files=[]
for p in sorted(target.rglob('*')):
 if p.is_file():
  old=source/p.relative_to(target);assert sha(p)==sha(old);files.append(dict(path=p.relative_to(target).as_posix(),bytes=p.stat().st_size,sha256=sha(p)))
(r/'source-inputs.json').write_bytes((json.dumps(dict(sourceLabel='joint-lit-surfaces-input',sourceProject=str(source),project=str(target/'playable.azureproject'),files=files),indent=2)+'\n').encode())
shutil.copy2(target/'scene/playable.azurelevel',r/'level-before-save.json')
actions=[dict(frame=1,command='save')];(r/'editor-actions.json').write_bytes((json.dumps(actions,indent=2)+'\n').encode())
print(json.dumps(dict(status='prepared',files=len(files),projectSha256=sha(target/'playable.azureproject'),levelSha256=sha(target/'scene/playable.azurelevel'))))
