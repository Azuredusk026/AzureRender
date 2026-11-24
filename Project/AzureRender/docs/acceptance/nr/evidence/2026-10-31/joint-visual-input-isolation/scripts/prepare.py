from pathlib import Path
import copy, hashlib, json, os, shutil

root=Path.cwd(); run=Path(__file__).resolve().parent
source=root/'build/runs/20261031-172112-hair-source-inputs-14d199/project'
target=run/'project'; target.mkdir()
binary={'.glb','.bin','.dds','.png','.jpg','.jpeg','.azlut','.azirr','.azfog','.azic','.azoa','.wav','.ogg','.fbx','.azprobe','.ktx2'}
for p in source.rglob('*'):
    if not p.is_file() or '.azure' in p.relative_to(source).parts: continue
    q=target/p.relative_to(source); q.parent.mkdir(parents=True,exist_ok=True)
    if p.suffix.lower() in binary: os.link(p,q)
    else: shutil.copy2(p,q)
spec=json.loads((run/'input-settings.json').read_text())
base=json.loads(Path(spec['near']['level']).read_text())
descriptor=json.loads((source/'near-restored.azureproject').read_text())
full=copy.deepcopy(spec['portrait']['renderSettings']['nprLighting'])
full['specularFloor']=base['renderSettings']['nprLighting']['specularFloor']
cases={}
cases['control']=copy.deepcopy(base)
cases['full-npr-real-shadow']=copy.deepcopy(base)
cases['full-npr-real-shadow']['renderSettings']['nprLighting']=copy.deepcopy(full)
cases['full-npr-real-shadow']['renderSettings']['nprLighting']['shadowOverride']=0
cases['full-npr-shadow-override']=copy.deepcopy(cases['full-npr-real-shadow'])
cases['full-npr-shadow-override']['renderSettings']['nprLighting']['shadowOverride']=1
cases['full-npr-portrait-display']=copy.deepcopy(cases['full-npr-real-shadow'])
for key in ['exposure','toneMapping','gradeLut','bloomEnabled']:
    cases['full-npr-portrait-display']['renderSettings'][key]=copy.deepcopy(spec['portrait']['renderSettings'][key])
rows=[]
for name, scene in cases.items():
    level=target/'scene'/f'{name}.azurelevel'
    level.write_text(json.dumps(scene,indent=2)+'\n')
    project=copy.deepcopy(descriptor);project['id']='visual-isolation-'+name;project['startupScene']='stage:/'+level.name
    path=target/(name+'.azureproject');path.write_text(json.dumps(project,indent=2)+'\n')
    rows.append({'name':name,'project':str(path),'levelSha256':hashlib.sha256(level.read_bytes()).hexdigest()})
(run/'cases.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Prepared',len(rows),'cases; frozen original assets preserved')
