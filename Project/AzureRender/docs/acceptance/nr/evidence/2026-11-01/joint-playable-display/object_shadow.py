import copy,hashlib,json,os,re,subprocess,time
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;project=run/'standalone-project';player=root/'build/ninja-msvc-release/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();save=lambda p,d:p.write_bytes((json.dumps(d,indent=2)+'\n').encode())
regions={'neck':(745,1020,815,1040),'chest-left':(455,1290,565,1370),'chest-center':(780,1210,850,1290),'shoulder-right':(1030,1250,1110,1340)}
prior=root/'build/runs/20261101-043757-material-display-transform-93d6e0';cases=[]
for name,base,on in [('world','near-world-linear',prior/'native-final-captures/near-world-linear'),('source','source-standalone',run/'standalone-captures/source-standalone')]:
 desc=json.loads((project/(base+'.azureproject')).read_text());scene=json.loads((project/'scene'/(base+'.azurelevel')).read_text());hero=next(n for n in scene['nodes'] if n['id']=='hero');assert hero['components']['azure.object-shadow']['data']['enabled'];hero['components']['azure.object-shadow']['data']['enabled']=False
 stem='object-shadow-disabled-'+name;desc.update(id=stem,name=stem,startupScene='stage:/'+stem+'.azurelevel');scene['id']=stem;path=project/(stem+'.azureproject');level=project/'scene'/(stem+'.azurelevel');save(path,desc);save(level,scene)
 cases.append(dict(name=name,project=str(path),level=str(level),baseline=str(on)))
inputs={p.relative_to(project).as_posix():sha(p) for p in project.rglob('*') if p.is_file() and '.azure' not in p.relative_to(project).parts};save(run/'object-shadow-inputs.json',dict(cases=cases,regions={key:list(value) for key,value in regions.items()},inputs=inputs))
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
report=dict(status='running',programSha256=sha(player),scope='Actual object-shadow component on/off in the combined world and original standalone lighting. Four skin rectangles were frozen before measurement.',cases=[],comparisons=[],visualAcceptance='failed-user-review');linear=lambda x:np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4);read=lambda p:linear(np.asarray(Image.open(p).convert('RGB'),dtype=np.float64)/255);responses={}
try:
 for case in cases:
  out=run/'object-shadow-captures'/case['name'];out.mkdir(parents=True);command=[str(player),'--project',case['project'],'--width','1536','--height','1536','--capture-frames','1','--fixed-frame-step','--capture-fps','60','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--runtime-report',str(out/'runtime.json')]
  row=dict(name=case['name'],command=command,status='running');report['cases'].append(row);start=time.perf_counter();print('START',case['name'],flush=True)
  with (out/'player.log').open('wb') as log:result=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
  row.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0
  assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(out/'player.log').read_text(errors='replace'),re.I)
  logs=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()];assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in logs);assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in logs)
  manifest=json.loads((out/'frames/capture_manifest.json').read_text());baseline=json.loads((Path(case['baseline'])/'frames/capture_manifest.json').read_text());assert manifest['objectShadowPages']==0 and baseline['objectShadowPages']==1
  for key in ['nprLighting','gradeSettings','faceSdfSettings','bloomSettings','qaCameraPosition','qaCameraTarget','antiAliasing','materialDisplayTransforms','materialRadianceScales']:assert manifest[key]==baseline[key],key
  runtime=json.loads((out/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
  a=read(Path(case['baseline'])/'frames/frame_000000.png');b=read(out/'frames/frame_000000.png');responses[case['name']]=a-b;row.update(status='captured',baselineSha256=sha(Path(case['baseline'])/'frames/frame_000000.png'),imageSha256=sha(out/'frames/frame_000000.png'),maximumImageResponse=float(np.max(np.abs(a-b))),changedPixels=int(np.any(a!=b,axis=2).sum()));print('DONE',case['name'],row['seconds'],row['maximumImageResponse'],flush=True)
 for name,(x0,y0,x1,y1) in regions.items():
  reference=responses['source'][y0:y1,x0:x1];world=responses['world'][y0:y1,x0:x1];error=float(np.max(np.abs(reference-world)));report['comparisons'].append(dict(region=name,bounds=[x0,y0,x1,y1],referenceMaximumResponse=float(np.max(np.abs(reference))),worldMaximumResponse=float(np.max(np.abs(world))),maximumResponseError=error,passed=error<=.008))
 for name,digest in inputs.items():assert sha(project/name)==digest,name
 report.update(status='measured',responseBudgetPassed=all(r['passed'] for r in report['comparisons']),hasNonzeroSkinResponse=any(r['referenceMaximumResponse']>0 and r['worldMaximumResponse']>0 for r in report['comparisons']));assert sha(player)==report['programSha256']
finally:
 if report['status']=='running':report['status']='failed'
 save(run/'object-shadow-summary.json',report);print(json.dumps({k:v for k,v in report.items() if k not in {'cases','comparisons'}}))
