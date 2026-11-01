import copy,hashlib,json,os,shutil
from pathlib import Path
run=Path(__file__).resolve().parent;project=run/'project';target=run/'diagnostic-project-v2';assert not target.exists();immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azirr','.azlut','.azfog','.hdr','.fbx','.wav','.ogg','.azprobe'}
for directory,dirs,files in os.walk(project):
 dirs[:]=[d for d in dirs if d!='.azure']
 for name in files:
  p=Path(directory)/name;q=target/p.relative_to(project);q.parent.mkdir(parents=True,exist_ok=True)
  if p.suffix.lower() in immutable:os.link(p,q)
  else:shutil.copy2(p,q)
base=json.loads((project/'scene/fullbody-world-aa0.azurelevel').read_text());descriptor=json.loads((project/'fullbody-world-aa0.azureproject').read_text());cases=[];save=lambda p,d:p.write_bytes((json.dumps(d,indent=2)+'\n').encode())
for variant,key in [('no-fog','viewFog'),('no-probes','reflectionProbe'),('no-gi','irradianceVolume')]:
 d=copy.deepcopy(base);assert key in d;d.pop(key);name='fullbody-'+variant;d['id']=name;desc=copy.deepcopy(descriptor);desc.update(id=name,name=name,startupScene='stage:/'+name+'.azurelevel');save(target/(name+'.azureproject'),desc);save(target/'scene'/(name+'.azurelevel'),d);cases.append(dict(name=name,kind='diagnostic',aa=0,frames=1,width=1536,height=1536,removedBinding=key,project=str(target/(name+'.azureproject'))))
for name in ['diagnostic-inputs.json','diagnostic-capture-summary.json','diagnostic-driver.log','diagnostic-captures']:
 p=run/name;q=run/('failed-relative-'+name);assert p.resolve().parent==run.resolve() and q.resolve().parent==run.resolve() and not q.exists()
 if p.exists():p.rename(q)
save(run/'diagnostic-inputs.json',dict(status='prepared',projectRoot=target.name,cases=cases,inputs={p.relative_to(target).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in target.rglob('*') if p.is_file()}));print('Prepared three single-variable diagnostics')
