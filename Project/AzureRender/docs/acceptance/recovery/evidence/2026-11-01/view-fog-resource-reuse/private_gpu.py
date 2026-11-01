import hashlib,json,os,subprocess,sys,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;assert json.loads((run/'public-gpu-summary.json').read_text())['status']=='passed';env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';env['PYTHONUTF8']='1';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();records=[]
prior=root/'build/runs/20261101-082926-joint-playable-display-c26c1b'
for cfg in ['release','debug']:
 build=json.loads((run/(cfg+'-build.json')).read_text());player=root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe';assert sha(player)==build['programs']['AzurePlayer'];previous=json.loads((prior/'build-reuse.json').read_text())['configurations'][cfg];assert build['shaders']==previous['shaders']
 command=[sys.executable,'-X','utf8',str(run/'capture_route.py'),'--config',cfg];print('START ROUTE',cfg,flush=True);start=time.perf_counter()
 with (run/('route-'+cfg+'-driver.log')).open('wb') as log:result=subprocess.run(command,cwd=root,stdout=log,stderr=log,env=env,timeout=1900)
 assert result.returncode==0
 folder=run/('route-'+cfg);summary=json.loads((folder/'summary.json').read_text());assert summary['status']=='behavior-passed-visual-review-pending' and summary['programSha256']==build['programs']['AzurePlayer']
 baseline=json.loads((prior/('route-'+cfg)/'frame-fingerprints.json').read_text());images={p.name:sha(p) for p in sorted((folder/'frames').glob('frame_*.png'))};assert len(images)==540
 different=[name for name in images if images[name]!=baseline[name]]
 rows=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()];preparations=[row['message'] for row in rows if row['message'].startswith('Scene load CPU preparation:')]
 row=dict(configuration=cfg,status='captured',programSha256=sha(player),elapsedSeconds=summary['elapsedSeconds'],frames=540,baselineIdenticalImages=540-len(different),differentImages=different,rendererCpuPreparations=len(preparations),preparationMessages=preparations);records.append(row);print('DONE ROUTE',cfg,'identical',row['baselineIdenticalImages'],'preparations',len(preparations),flush=True)
 (run/'private-gpu-summary.json').write_bytes((json.dumps(dict(status='running',checks=records),indent=2)+'\n').encode())
 assert len(preparations)==1,'Identical fog restart still prepares another renderer'
 assert not different,'Frozen full-scene route changed after the reuse fix'
(run/'private-gpu-summary.json').write_bytes((json.dumps(dict(status='passed',checks=records),indent=2)+'\n').encode())
