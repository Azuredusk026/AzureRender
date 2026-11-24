import hashlib,json,os,re,shutil,subprocess,time
from pathlib import Path
import numpy as np
from PIL import Image
run=Path(__file__).resolve().parent;root=run.parents[2];source=run/'candidate-project';frozen=json.loads((run/'lighting-candidate.json').read_text());build=json.loads((run/'release-build.json').read_text());player=root/'build/ninja-msvc-release/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert sha(player)==build['programs']['AzurePlayer'];assert {p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')}==build['shaders'];out=run/'repeat-isolations';out.mkdir(exist_ok=False);immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azirr','.azlut','.azfog','.hdr','.fbx','.wav','.ogg','.azprobe'};projects={};report=dict(status='preparing',hypothesis='Projected reflection history or edge resolve amplifies a subpixel input difference.',programSha256=sha(player),sourceManifestSha256=sha(run/'lighting-candidate.json'),projects=[],cases=[],comparisons=[]);env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
def progress():(run/'repeat-isolations.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
progress()
try:
 for mode in ['default','no-ssr','aa-off']:
  target=out/'projects'/mode
  for name,digest in frozen['inputs'].items():
   p=source/name;q=target/name;q.parent.mkdir(parents=True,exist_ok=True)
   if p.suffix.lower() in immutable:os.link(p,q);assert os.path.samefile(p,q)
   else:assert sha(p)==digest,name;shutil.copy2(p,q)
  p=target/'scene/full-world-hidden.azurelevel';d=json.loads(p.read_text());settings=d['renderSettings']
  if mode=='no-ssr':assert settings['screenSpaceReflection']['enabled'];settings['screenSpaceReflection']['enabled']=False
  if mode=='aa-off':assert settings['antiAliasing']==1;settings['antiAliasing']=0
  p.write_bytes((json.dumps(d,ensure_ascii=False,indent=2)+'\n').encode());projects[mode]=target;report['projects'].append(dict(mode=mode,levelSha256=sha(p),antiAliasing=settings['antiAliasing'],screenSpaceReflection=settings['screenSpaceReflection']));progress()
 report['status']='capturing';progress()
 for mode in projects:
  for repeat in ['a','b']:
   name=mode+'-'+repeat;case=out/name;case.mkdir();command=[str(player),'--project',str(projects[mode]/'full-world-hidden.azureproject'),'--width','2560','--height','1440','--capture-frames','12','--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--capture-dir',str(case/'frames'),'--runtime-report',str(case/'runtime.json')];row=dict(name=name,status='running',command=command);report['cases'].append(row);progress();start=time.perf_counter();print('START',name,flush=True)
   with (case/'player.log').open('wb') as log:result=subprocess.run(command,cwd=case,env=env,stdout=log,stderr=log,startupinfo=startup,timeout=700)
   row.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0,name;logs=[json.loads(s) for s in (case/'captures/azureplayer.log.jsonl').read_text().splitlines()];assert not any(v['level']=='error' or v['subsystem']=='validation' and v['level']=='warning' for v in logs);assert any(v['message']=='Allocator after unload: buffers=0 images=0' for v in logs);assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(case/'player.log').read_text(errors='replace'),re.I);assert len(list((case/'frames').glob('frame_*.png')))==12;row['status']='captured';progress();print('DONE',name,round(row['seconds'],2),flush=True)
  differences=[]
  for i in range(12):
   name='frame_%06d.png'%i;a=np.asarray(Image.open(out/(mode+'-a')/'frames'/name));b=np.asarray(Image.open(out/(mode+'-b')/'frames'/name));delta=abs(a.astype(np.int16)-b.astype(np.int16));mask=np.any(delta[:,:,:3]!=0,axis=2)
   if np.any(mask):differences.append(dict(frame=i,pixels=int(mask.sum()),maximumByteError=int(delta.max())))
  report['comparisons'].append(dict(mode=mode,differentFrames=differences,exact=not differences));progress()
 assert sha(player)==report['programSha256'];report['status']='measured-repeat-isolations';progress()
except Exception as error:report.update(status='failed',error=repr(error));progress();raise
