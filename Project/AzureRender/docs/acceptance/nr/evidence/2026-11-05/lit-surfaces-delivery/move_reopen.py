from pathlib import Path
import hashlib,json,os,re,subprocess,time
from PIL import Image
import numpy as np
root=Path.cwd();r=Path(__file__).resolve().parent;source=(r/'source-project').resolve();target=(r/'移动后的完整场景').resolve();assert source.is_relative_to(r.resolve()) and target.is_relative_to(r.resolve()) and source!=r.resolve() and not target.exists();assert json.loads((r/'editor-save/summary.json').read_text())['status']=='saved-semantic-review-pending';source.rename(target)
out=r/'moved-reopen';out.mkdir();project=target/'playable.azureproject';player=r/'engine/bin/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
command=[str(player),'--project',str(project),'--width','960','--height','540','--capture-fps','60','--fixed-frame-step','--capture-frames','3','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty','--runtime-report',str(out/'runtime.json')]
report=dict(status='running',command=command,programSha256=sha(player),projectSha256=sha(project),levelSha256=sha(target/'scene/playable.azurelevel'),movedDirectory=True,originalPathUnavailable=not source.exists());env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;t=time.perf_counter()
try:
 with (out/'player.log').open('wb') as log:p=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=900)
 report.update(exitCode=p.returncode,elapsedSeconds=time.perf_counter()-t);assert p.returncode==0
 log=(out/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log and not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
 d=json.loads((out/'runtime.json').read_text());assert not d['scriptErrors'] and not d['presentationErrors'];frames=[]
 for i in range(3):
  before=r.parent/f'lit-surfaces-game-release/frames/frame_{i:06d}.png';after=out/f'frames/frame_{i:06d}.png';a=np.asarray(Image.open(before).convert('RGBA'));b=np.asarray(Image.open(after).convert('RGBA'));assert a.shape==b.shape;changed=int(np.count_nonzero(np.any(a!=b,axis=2)));frames.append(dict(frame=i,changedPixels=changed,referenceSha256=sha(before),actualSha256=sha(after)));assert changed==0,frames[-1]
 report.update(status='passed',frames=frames);assert sha(player)==report['programSha256']
finally:
 if report['status']=='running':report['status']='failed'
 (out/'summary.json').write_bytes((json.dumps(report,indent=2,ensure_ascii=False)+'\n').encode());print(json.dumps(report,indent=2,ensure_ascii=False))
