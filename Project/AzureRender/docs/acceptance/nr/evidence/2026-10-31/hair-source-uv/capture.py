from pathlib import Path
import subprocess,json,os,re,time,hashlib
root=Path.cwd();r=Path(__file__).resolve().parent;reference=root/'build/runs/20261031-164808-character-frontal-lighting-fa3879';base=json.loads((reference/'summary.json').read_text())['cases'][0]['command'];project=root/json.loads((r/'candidate.json').read_text())['project'];env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;report=dict(status='running',cases=[]);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();frozen=sha(Path(base[0]))
try:
 for isolation in ['beauty','albedo']:
  out=r/isolation;out.mkdir();command=base.copy()
  for key,value in {'--project':project,'--capture-dir':out/'frames','--runtime-report':out/'runtime.json','--qa-isolation':isolation}.items():command[command.index(key)+1]=str(value)
  row=dict(case=isolation,status='running',command=command,programSha256=frozen,projectSha256=sha(project));report['cases'].append(row);(r/'capture-summary.json').write_text(json.dumps(report,indent=2)+'\n');print('START',isolation,flush=True);t=time.perf_counter()
  with (out/'player.log').open('wb') as log:p=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=600)
  row.update(exitCode=p.returncode,elapsedSeconds=time.perf_counter()-t);assert p.returncode==0;log=(out/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log and not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I);m=json.loads((out/'frames/capture_manifest.json').read_text());assert m['antiAliasing']==0 and m['capturedFrames']==1 and m['qaIsolation'].lower()==isolation;runtime=json.loads((out/'runtime.json').read_text());assert runtime['scriptErrors']==[] and runtime['presentationErrors']==[];assert sha(Path(base[0]))==frozen and sha(project)==row['projectSha256'];row['capture']=m;row['status']='captured';(r/'capture-summary.json').write_text(json.dumps(report,indent=2)+'\n');print('DONE',isolation,flush=True)
 report['status']='captured-diagnostic-visual-review-pending'
finally:(r/'capture-summary.json').write_text(json.dumps(report,indent=2)+'\n')
