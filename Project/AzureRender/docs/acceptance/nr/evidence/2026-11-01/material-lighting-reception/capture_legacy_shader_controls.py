import hashlib,json,os,re,subprocess,time
from pathlib import Path
import numpy as np
from PIL import Image
run=Path(__file__).resolve().parent;root=run.parents[2];build=json.loads((run/'release-build.json').read_text());controls=json.loads((run/'legacy-shader-control-build.json').read_text());assert controls['status']=='compiled-byte-exact-legacy-shader-controls';player=root/'build/ninja-msvc-release/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert sha(player)==build['programs']['AzurePlayer'];out=run/'legacy-shader-captures';out.mkdir(exist_ok=False);frozen=root/'build/runs/20261101-043757-material-display-transform-93d6e0/native-final-captures/full-world-hidden/frames';report=dict(status='running',programSha256=sha(player),cases=[],comparisons=[],visualAcceptance='failed-user-review');startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
def progress():(run/'legacy-shader-captures.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
progress()
try:
 for mode,resource in controls['shaderRoots'].items():
  env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';env['AZURERENDER_RESOURCE_ROOT']=resource
  for repeat in ['a','b']:
   name=mode+'-'+repeat;folder=out/name;folder.mkdir();command=[str(player),'--project',str(run/'candidate-project/full-world-hidden.azureproject'),'--width','2560','--height','1440','--capture-frames','12','--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--capture-dir',str(folder/'frames'),'--runtime-report',str(folder/'runtime.json')];row=dict(name=name,status='running',command=command,resourceRoot=resource);report['cases'].append(row);progress();start=time.perf_counter();print('START',name,flush=True)
   with (folder/'player.log').open('wb') as log:result=subprocess.run(command,cwd=folder,env=env,stdout=log,stderr=log,startupinfo=startup,timeout=700)
   row.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0,name;logs=[json.loads(s) for s in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()];assert not any(v['level']=='error' or v['subsystem']=='validation' and v['level']=='warning' for v in logs);assert any(v['message']=='Allocator after unload: buffers=0 images=0' for v in logs);assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(folder/'player.log').read_text(errors='replace'),re.I);assert len(list((folder/'frames').glob('frame_*.png')))==12
   changes=[]
   for image in sorted((folder/'frames').glob('frame_*.png')):
    a=np.asarray(Image.open(image));b=np.asarray(Image.open(frozen/image.name));d=abs(a.astype(np.int16)-b.astype(np.int16));mask=np.any(d[:,:,:3]!=0,axis=2)
    if np.any(mask):changes.append(dict(frame=image.name,pixels=int(mask.sum()),maximumByteError=int(d.max())))
   row.update(status='captured',versusFrozenDifferentFrames=changes);progress();print('DONE',name,round(row['seconds'],2),'frozen differences',len(changes),flush=True)
  changed=[]
  for i in range(12):
   image='frame_%06d.png'%i
   if sha(out/(mode+'-a')/'frames'/image)!=sha(out/(mode+'-b')/'frames'/image):changed.append(image)
  report['comparisons'].append(dict(mode=mode,differentRepeatFrames=changed,exactRepeat=not changed));progress()
 assert sha(player)==report['programSha256'];report['status']='measured-legacy-shader-controls';progress()
except Exception as error:report.update(status='failed',error=repr(error));progress();raise
