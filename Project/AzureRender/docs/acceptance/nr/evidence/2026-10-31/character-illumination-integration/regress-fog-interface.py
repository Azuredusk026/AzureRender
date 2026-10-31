from pathlib import Path
import subprocess,json,time,os,re,hashlib
import numpy as np
from PIL import Image
root=Path.cwd();r=Path(__file__).resolve().parent;out=r/'fog-interface-regression';out.mkdir();env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();report={'status':'running','cases':[]}
def execute(name,command,baseline=None,validation=False):
 target=out/name;target.mkdir();row=dict(case=name,command=command,status='running',playerSha256=sha(Path(command[0])) if name!='release-fog-fixtures' else sha(root/'build/ninja-msvc-release/AzurePlayer.exe'));report['cases'].append(row);(out/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print('START',name,flush=True);t=time.perf_counter()
 with (target/'player.log').open('wb') as log:p=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=900)
 row.update(exitCode=p.returncode,elapsedSeconds=time.perf_counter()-t);assert p.returncode==0,(name,p.returncode)
 if baseline is not None:
  text=(target/'player.log').read_text(encoding='utf-8',errors='replace');assert 'Allocator after unload: buffers=0 images=0' in text and not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text,re.I),name
  assert ('Validation layer: enabled' in text)==validation
  m=json.loads((target/'frames/capture_manifest.json').read_text());assert m['capturedFrames']==int(command[command.index('--capture-frames')+1]);row['capture']=m
  runtime=json.loads((target/'runtime.json').read_text());assert runtime['scriptErrors']==[] and runtime['presentationErrors']==[]
  compared=[]
  for actual in sorted((target/'frames').glob('frame_*.png')):
   previous=baseline/actual.name;assert previous.exists();x=np.asarray(Image.open(actual).convert('RGB'));y=np.asarray(Image.open(previous).convert('RGB'));d=np.abs(x.astype('i2')-y.astype('i2'));compared.append(dict(frame=actual.name,pngBytesEqual=actual.read_bytes()==previous.read_bytes(),changedPixels=int(np.any(d>0,axis=2).sum()),maximumByteDifference=int(d.max())))
  row['imageComparison']=compared;row['strictImagesEqual']=all(x['pngBytesEqual'] for x in compared)
 row['status']='passed' if baseline is None or row['strictImagesEqual'] else 'strict-image-compatibility-failed';(out/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print('DONE',name,row['status'],flush=True)
try:
 command=[str(root/'build/reinstall-environment/venv/Scripts/python.exe'),'-X','utf8',str(root/'tools/test_view_fog_gpu.py'),'--executable',str(root/'build/ninja-msvc-release/AzurePlayer.exe'),'--output',str(out/'release-fixtures'),'--expected-validation','disabled'];execute('release-fog-fixtures',command)
 original=json.loads((r/'illumination-full-summary.json').read_text());base=next(x['command'] for x in original['cases'] if x['case']=='full')
 for name,config,validation in [('full-release','release',False),('full-debug','debug',True)]:
  target=out/name;cmd=base.copy();cmd[0]=str(root/('build/ninja-msvc-'+config+'/AzurePlayer.exe'))
  for k,v in {'--capture-dir':target/'frames','--runtime-report':target/'runtime.json'}.items():cmd[cmd.index(k)+1]=str(v)
  execute(name,cmd,r/'illumination-full/frames',validation)
 source=root/'build/runs/20261024-074605-native-material-aa-reference-9eb92d';reproduced=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference'
 for kind in ['temporal','high']:
  name='portrait-'+kind;target=out/name;cmd=json.loads((source/kind/'command.json').read_text());cmd[0]=str(root/'build/ninja-msvc-release/AzurePlayer.exe')
  for k,v in {'--project':reproduced/'project'/(kind+'.azureproject'),'--capture-dir':target/'frames','--runtime-report':target/'runtime.json','--gpu-timing-output':target/'gpu-timing.json'}.items():cmd[cmd.index(k)+1]=str(v)
  execute(name,cmd,reproduced/kind/'frames')
 report['status']='passed' if all(x['status']=='passed' for x in report['cases']) else 'strict-image-compatibility-pending'
finally:(out/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
