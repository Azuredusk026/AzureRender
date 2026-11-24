import argparse,hashlib,json,subprocess,sys,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--resume',action='store_true');parser.add_argument('--retry-suffix',default='retry1');parser.add_argument('--refresh-fog',action='store_true');args=parser.parse_args()
steps=[]
for script in ['backend_gpu.py','supersample_gpu.py','related_gpu.py']:
 for cfg in ['release','debug']:steps.append((script.removesuffix('.py')+'-'+cfg,[sys.executable,'-X','utf8',str(run/script),'--configuration',cfg],12000 if script=='related_gpu.py' else 1000))
steps.append(('view-gpu-debug-final',[sys.executable,'-X','utf8',str(run/'run_view_gpu.py'),'--configuration','debug','--suffix','debug-final'],1100))
steps.append(('native-final',[sys.executable,'-X','utf8',str(run/'capture_native_final.py')],4500))
programs={cfg:hashlib.sha256((root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe').read_bytes()).hexdigest() for cfg in ['release','debug']}
report=dict(status='running',programs=programs,steps=[])
if args.resume:
 previous=run/'remaining-summary.json';initial=json.loads(previous.read_text());assert initial['programs']==programs
 (run/('remaining-before-resume-'+str(time.time_ns())+'-summary.json')).write_bytes(previous.read_bytes())
 report['steps']=[row for row in initial['steps'] if row.get('status')=='passed']
 for row in list(report['steps']):
  if row['name'].startswith('related_gpu-'):
   cfg=row['name'].removeprefix('related_gpu-');child=json.loads((run/('related-gpu-'+cfg+'-summary.json')).read_text())
   if args.refresh_fog or any(check['scriptSha256']!=hashlib.sha256(Path(check['command'][3]).read_bytes()).hexdigest() for check in child['checks']):report['steps'].remove(row)
try:
 for name,command,timeout in steps:
  if any(row['name']==name for row in report['steps']):print('REUSED',name,flush=True);continue
  if args.resume and name.startswith('related_gpu-'):
   cfg=name.removeprefix('related_gpu-');previous=run/('related-gpu-'+cfg+'-summary.json')
   if previous.exists():command+=['--resume','--retry-suffix',args.retry_suffix]
   if args.refresh_fog:command+=['--refresh','scene-fog','--refresh','fog-msaa']
  row=dict(name=name,command=command,status='running');report['steps'].append(row);print('START',name,flush=True);start=time.perf_counter()
  with (run/(name+'-driver.log')).open('wb') as log:result=subprocess.run(command,cwd=root,stdout=log,stderr=log,timeout=timeout)
  row.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0,row
  for cfg,digest in programs.items():assert hashlib.sha256((root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe').read_bytes()).hexdigest()==digest,cfg
  row['status']='passed';print('DONE',name,flush=True);(run/'remaining-summary.json').write_text(json.dumps(report,indent=2)+'\n')
 report['status']='passed'
except Exception as error:report.update(status='failed',failure=str(error));raise
finally:(run/'remaining-summary.json').write_text(json.dumps(report,indent=2)+'\n')
