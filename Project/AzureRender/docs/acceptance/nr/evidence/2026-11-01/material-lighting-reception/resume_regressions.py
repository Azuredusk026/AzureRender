import json,subprocess,sys,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;report=dict(status='running',jobs=[])
jobs=[('related-release-resume','related_gpu.py',['--configuration','release',*[arg for n in ['object-shadow','npr','specular-ramp','view-fog','scene-fog'] for arg in ['--suite',n]]]),('related-debug','related_gpu.py',['--configuration','debug']),('route-release','capture_route.py',['--config','release']),('route-debug','capture_route.py',['--config','debug'])]
try:
 for name,script,args in jobs:
  row=dict(name=name,status='running');report['jobs'].append(row);print('START',name,flush=True);start=time.perf_counter()
  with (run/(name+'-driver.log')).open('wb') as log:r=subprocess.run([sys.executable,'-X','utf8',str(run/script),*args],cwd=root,stdout=log,stderr=log,timeout=2400)
  row.update(exitCode=r.returncode,seconds=time.perf_counter()-start);assert r.returncode==0,name;row['status']='passed';print('DONE',name,flush=True)
  if name=='related-release-resume':
   old=json.loads((run/'failed-observer/related-release.json').read_text());fresh=json.loads((run/'related-release-object-shadow,npr,specular-ramp,view-fog,scene-fog.json').read_text());assert fresh['status']=='passed';old['checks']=[c for c in old['checks'] if c.get('status')=='passed']+fresh['checks'];assert len(old['checks'])==7;old['status']='passed';(run/'related-release.json').write_bytes((json.dumps(old,indent=2)+'\n').encode())
  (run/'resumed-regression-pipeline.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
 report['status']='passed'
finally:
 if report['status']=='running':report['status']='failed'
 (run/'resumed-regression-pipeline.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
