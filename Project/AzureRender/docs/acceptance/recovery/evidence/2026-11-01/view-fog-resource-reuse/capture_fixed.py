import hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;player=root/'build/ninja-msvc-release/AzurePlayer.exe';project=run/'project/full-world-linear.azureproject';out=run/'fixed-world';out.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();build=json.loads((run/'release-build.json').read_text());assert sha(player)==build['programs']['AzurePlayer']
command=[str(player),'--project',str(project),'--width','2560','--height','1440','--capture-frames','12','--fixed-frame-step','--capture-fps','60','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--runtime-report',str(out/'runtime.json')]
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;record=dict(status='running',command=command,programSha256=sha(player));start=time.perf_counter()
try:
 with (out/'player.log').open('wb') as log:result=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
 record.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0
 assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(out/'player.log').read_text(errors='replace'),re.I)
 logs=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()];assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in logs);assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in logs)
 manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==12 and manifest['materialSurfaceProfileLayoutVersion']==19 and len(manifest['materialDisplayTransforms'])==11
 prior=root/'build/runs/20261101-043757-material-display-transform-93d6e0/native-final-captures/full-world-linear/frames';images={p.name:sha(p) for p in sorted((out/'frames').glob('frame_*.png'))};assert len(images)==12
 assert all(sha(prior/name)==digest for name,digest in images.items())
 runtime=json.loads((out/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors'];assert sha(player)==record['programSha256']
 record.update(status='passed',pixelExactFrames=12,images=images)
finally:
 if record['status']=='running':record['status']='failed'
 (run/'fixed-world-summary.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in record.items() if k not in {'command','images'}}))
