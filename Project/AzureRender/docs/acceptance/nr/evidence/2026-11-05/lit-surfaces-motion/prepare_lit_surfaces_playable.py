from pathlib import Path
import os,json,shutil,hashlib
r=Path(__file__).resolve().parent;source=r/'qualified-lit-surfaces-joint-v4';p=r/'qualified-lit-surfaces-playable';shutil.copytree(r/'skin-warm-playable',p,copy_function=os.link,ignore=shutil.ignore_patterns('.azure','captures','output'))
files=['assets/linear-display-materials/material-1.azurematerial','assets/linear-display-materials/material-2.azurematerial','assets/linear-display-materials/material-9.azurematerial','content-character/hero-idle-opaque-overlays.glb']
files+=[v['path'] for v in json.loads((source/'hair-source-samplers.json').read_bytes())['records']]
files+=[f.relative_to(source).as_posix() for f in (source/'content-character/derived-textures').rglob('*') if f.is_file()]
for rel in sorted(set(files)):
 dest=p/rel;dest.parent.mkdir(parents=True,exist_ok=True);tmp=dest.with_suffix(dest.suffix+'.update');shutil.copyfile(source/rel,tmp);os.replace(tmp,dest)
level=json.loads((p/'scene/playable.azurelevel').read_bytes());hero=next(n for n in level['nodes'] if n.get('id')=='hero');assert len(hero['materialSlots'])==14
for index in [0,8]:assert json.loads((p/f'assets/linear-display-materials/material-{index}.azurematerial').read_bytes())['surface']['materials'][0]['pbrMetallicRoughness']['baseColorFactor']==[.98,.93,.89,1]
(p/'derived-surfaces-playable.json').write_text(json.dumps(dict(status='prepared',sourceProject='qualified-lit-surfaces-joint-v4',sourceMaterialSlots=14,records=[dict(path=rel,sha256=hashlib.sha256((p/rel).read_bytes()).hexdigest()) for rel in sorted(set(files))]),indent=2));print('playable assets',len(set(files)),flush=True)
