import json,subprocess,sys,time
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];out=run/'shader-family-revision'
report=dict(status='waiting-for-build-and-release-public',jobs=[])
def save():(out/'pipeline.json').write_text(json.dumps(report,indent=2)+'\n')
save();deadline=time.monotonic()+1800
while True:
    ready=True
    for path in [run/'debug-build.json',out/'public-gpu-release.json']:
        if not path.exists():ready=False;continue
        data=json.loads(path.read_text())
        assert data['status']!='failed',str(path)
        if data['status'] not in ['passed','built-contract-tests-passed']:ready=False
    if ready:break
    assert time.monotonic()<deadline,'Prerequisite deadline exceeded'
    time.sleep(5)
jobs=[('debug-programs','check_family_programs.py',['--configuration','debug']),
      ('public-debug','family_public_gpu.py',['--configuration','debug']),
      ('native','family_capture_native.py',[]),
      ('compatibility','family_compare_compatibility.py',[]),
      ('body-aa0','family_compare_body.py',['--aa','0']),
      ('body-aa3','family_compare_body.py',['--aa','3']),
      ('related-release','family_related_gpu.py',['--configuration','release']),
      ('related-debug','family_related_gpu.py',['--configuration','debug']),
      ('route-release','family_capture_route.py',['--config','release']),
      ('route-debug','family_capture_route.py',['--config','debug'])]
report['status']='running';save()
try:
    for name,script,args in jobs:
        row=dict(name=name,status='running');report['jobs'].append(row);save()
        print('START',name,flush=True);start=time.perf_counter()
        with (out/(name+'-driver.log')).open('wb') as log:
            result=subprocess.run([sys.executable,'-X','utf8',str(run/script),*args],cwd=root,stdout=log,stderr=log,timeout=3600)
        row.update(exitCode=result.returncode,seconds=time.perf_counter()-start)
        assert result.returncode==0,name
        if name=='compatibility':
            measured=json.loads((out/'compatibility-comparison.json').read_text())
            assert measured['strictCompatibilityPassed'] and measured['headCriteriaPassed'],name
        row['status']='passed';save();print('DONE',name,round(row['seconds'],2),flush=True)
    report['status']='passed';save()
except Exception as error:
    report.update(status='failed',error=repr(error));save();raise
