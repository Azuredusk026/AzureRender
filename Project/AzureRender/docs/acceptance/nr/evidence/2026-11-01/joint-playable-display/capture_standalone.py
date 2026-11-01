import hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;spec=json.loads((run/'standalone-inputs.json').read_text());player=root/'build/ninja-msvc-release/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
report=dict(status='running',programSha256=sha(player),cases=[],visualAcceptance='failed-user-review')
try:
 for case in spec['cases']:
  out=run/'standalone-captures'/case['name'];out.mkdir(parents=True)
  command=[str(player),'--project',case['project'],'--width','1536','--height','1536','--capture-frames','1','--fixed-frame-step','--capture-fps','60','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--runtime-report',str(out/'runtime.json')]
  row=dict(name=case['name'],command=command,status='running');report['cases'].append(row);start=time.perf_counter();print('START',case['name'],flush=True)
  with (out/'player.log').open('wb') as log:result=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
  row.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0
  assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(out/'player.log').read_text(errors='replace'),re.I)
  rows=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()]
  assert not any(row['level']=='error' or row['subsystem']=='validation' and row['level']=='warning' for row in rows)
  assert any(row['message']=='Allocator after unload: buffers=0 images=0' for row in rows)
  runtime=json.loads((out/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
  manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==1 and manifest['sceneWorldCoordinates']
  assert manifest['materialSurfaceProfileLayoutVersion']==19 and len(manifest['materialDisplayTransforms'])==11
  row.update(status='captured',imageSha256=sha(out/'frames/frame_000000.png'),manifest=manifest);print('DONE',case['name'],row['seconds'],flush=True)
  (run/'standalone-capture-summary.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
 for name,digest in spec['inputs'].items():assert sha(run/'standalone-project'/name)==digest,name
 assert sha(player)==report['programSha256'];report['status']='captured-inputs-unchanged'
finally:
 if report['status']=='running':report['status']='failed'
 (run/'standalone-capture-summary.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(report['status'])
