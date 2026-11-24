import hashlib,json,subprocess,sys,time
from pathlib import Path

root=Path.cwd();run=Path(__file__).resolve().parent
deadline=time.monotonic()+4800
while True:
    try:
        native=json.loads((run/'native-capture-summary.json').read_text())
        ready=native['status']=='captured-inputs-unchanged' and len(native['cases'])==8
        if native['status']=='failed':raise RuntimeError('Native capture failed; dependent GPU work stopped')
        if ready:break
    except (FileNotFoundError,json.JSONDecodeError):pass
    assert time.monotonic()<deadline,'Native capture completion deadline exceeded'
    time.sleep(15)

report=dict(status='running',jobs=[])
jobs=[('compare-aa0','compare_body.py',['--aa','0']),('compare-aa3','compare_body.py',['--aa','3']),('compatibility','compare_compatibility.py',[]),('related-release','related_gpu.py',['--configuration','release']),('related-debug','related_gpu.py',['--configuration','debug']),('route-release','capture_route.py',['--config','release']),('route-debug','capture_route.py',['--config','debug'])]
try:
    for name,script,args in jobs:
        command=[sys.executable,'-X','utf8',str(run/script),*args]
        row=dict(name=name,command=command,status='running');report['jobs'].append(row)
        print('START',name,flush=True);start=time.perf_counter()
        with (run/(name+'-driver.log')).open('wb') as log:
            result=subprocess.run(command,cwd=root,stdout=log,stderr=log,timeout=2000)
        row.update(exitCode=result.returncode,seconds=time.perf_counter()-start)
        assert result.returncode==0,name
        row['status']='passed';print('DONE',name,round(row['seconds'],2),flush=True)
        (run/'regression-pipeline.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    report['status']='passed'
finally:
    if report['status']=='running':report['status']='failed'
    (run/'regression-pipeline.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
