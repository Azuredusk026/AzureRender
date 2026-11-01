import hashlib,json,os,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;python=root/'build/reinstall-environment/venv/Scripts/python.exe'
spec=json.loads((run/'candidate.json').read_text());out=run/'project-check';out.mkdir()
player=root/'build/ninja-msvc-release/AzurePlayer.exe';command=[str(player),'--project',str(root/spec['project']),'--check-project']
start=time.perf_counter();env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';env['PYTHONUTF8']='1'
with (out/'player.log').open('wb') as log:result=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,timeout=1800)
record=dict(status='passed' if result.returncode==0 else 'failed',command=command,exitCode=result.returncode,seconds=time.perf_counter()-start,programSha256=hashlib.sha256(player.read_bytes()).hexdigest())
(out/'summary.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print('PROJECT CHECK',record['status'],record['seconds'],flush=True);assert result.returncode==0
for cfg in ['release','debug']:
 print('START ROUTE',cfg,flush=True)
 subprocess.run([str(python),'-X','utf8',str(run/'capture.py'),'--config',cfg],cwd=root,env=env,check=True)
 print('DONE ROUTE',cfg,flush=True)
