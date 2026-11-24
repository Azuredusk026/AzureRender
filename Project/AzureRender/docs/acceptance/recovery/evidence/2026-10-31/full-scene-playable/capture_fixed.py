from pathlib import Path
import subprocess,json,os,hashlib,re,time
from PIL import Image
import numpy as np
root=Path.cwd();run=Path(__file__).resolve().parent;source=root/'build/runs/20261031-172112-hair-source-inputs-14d199'
out=run/'fixed-regression';out.mkdir();project=run/'project/full-restored.azureproject';player=root/'build/ninja-msvc-release/AzurePlayer.exe'
assert project.read_bytes()==(source/'project/full-restored.azureproject').read_bytes()
command=[str(player),'--project',str(project),'--width','2560','--height','1440','--capture-frames','3','--capture-fps','60','--fixed-frame-step','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty','--runtime-report',str(out/'runtime.json')]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();report=dict(status='running',command=command,programSha256=sha(player),projectSha256=sha(project));env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;t=time.perf_counter()
try:
 with (out/'player.log').open('wb') as log:p=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=900)
 report.update(exitCode=p.returncode,elapsedSeconds=time.perf_counter()-t);assert p.returncode==0
 log=(out/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log and not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
 rows=[]
 for i in range(3):
  before=source/f'regression/full-restored/frames/frame_{i:06d}.png';after=out/f'frames/frame_{i:06d}.png';a=np.asarray(Image.open(before).convert('RGBA'));b=np.asarray(Image.open(after).convert('RGBA'));assert a.shape==b.shape;changed=int(np.count_nonzero(np.any(a!=b,axis=2)));rows.append(dict(frame=i,changedPixels=changed,referenceSha256=sha(before),actualSha256=sha(after)));assert changed==0,rows[-1]
 report.update(status='passed',frames=rows);assert sha(player)==report['programSha256'] and sha(project)==report['projectSha256']
finally:
 if report['status']=='running':report['status']='failed'
 (out/'summary.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report,indent=2))
