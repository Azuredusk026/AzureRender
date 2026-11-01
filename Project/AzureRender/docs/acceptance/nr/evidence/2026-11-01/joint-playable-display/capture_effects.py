import hashlib,json,os,re,subprocess,time
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;project=run/'project';prior=root/'build/runs/20261101-043757-material-display-transform-93d6e0'
player=root/'build/ninja-msvc-release/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(player)==json.loads((prior/'release-final-build.json').read_text())['programs']['AzurePlayer']
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
spec=json.loads((run/'candidate.json').read_text());records=[]
regions={'hair-crown':(710,400,820,550),'hair-fringe':(700,570,800,650),'face-cheek':(610,780,680,850),'face-chin':(680,940,770,985),'eye-left':(570,700,650,750)}
# Bounds are taken from the frozen native comparison, including any exact record corrections.
comparison=json.loads((prior/'native-final-comparison.json').read_text())
regions={row['region']:tuple(row['bounds']) for row in comparison['comparisons'][0]['regions']}
linear=lambda x:np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)
read=lambda p:linear(np.asarray(Image.open(p).convert('RGB'),dtype=np.float64)/255)
base={name:read(prior/'native-final-captures'/('near-'+name)/'frames/frame_000000.png') for name in ['world-linear','linear-isolated']}
baseline_metadata={name:json.loads((prior/'native-final-captures'/('near-'+name)/'frames/capture_manifest.json').read_text()) for name in base}
report=dict(status='running',programSha256=sha(player),cases=records,comparisons=[],visualAcceptance='failed-user-review',scope='Frozen five interior regions compare native effect response under the complete world presentation and isolated display control. World fog, grading, bloom and screen-space reflection remain enabled only in the world case. Whole-body, motion and final visual acceptance are separate.')
try:
 for effect in ['hair-kk','face-sdf','shadow','toon']:
  images={}
  for name in ['world-linear','linear-isolated']:
   out=run/'native-effects'/(name+'-'+effect+'-disabled');out.mkdir(parents=True)
   command=[str(player),'--project',str(project/('near-'+name+'.azureproject')),'--width','1536','--height','1536','--capture-frames','1','--fixed-frame-step','--capture-fps','60','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--qa-effect',effect,'--qa-effect-state','disabled','--runtime-report',str(out/'runtime.json')]
   record=dict(name=name,effect=effect,command=command,status='running');records.append(record);print('START',name,effect,flush=True);start=time.perf_counter()
   with (out/'player.log').open('wb') as log:result=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
   record.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0
   assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(out/'player.log').read_text(errors='replace'),re.I)
   rows=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()]
   assert not any(row['level']=='error' or row['subsystem']=='validation' and row['level']=='warning' for row in rows)
   assert any(row['message']=='Allocator after unload: buffers=0 images=0' for row in rows)
   runtime=json.loads((out/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
   manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==1 and len(manifest['materialDisplayTransforms'])==11
   assert manifest['qaEffect']==effect and manifest['qaEffectState']=='disabled' and manifest['qaIsolation']=='beauty'
   for key in ['nprLighting','gradeSettings','shadowSettings','faceSdfSettings','bloomSettings','cameraLens','qaCameraPosition','qaCameraTarget','antiAliasing','materialDisplayTransforms','materialRadianceScales']+[key for key in manifest if 'fog' in key.lower()]:
    assert manifest[key]==baseline_metadata[name][key],(name,effect,key)
   path=out/'frames/frame_000000.png';images[name]=read(path);record.update(status='captured',imageSha256=sha(path));print('DONE',name,effect,flush=True)
  for region,(x0,y0,x1,y1) in regions.items():
   crop=lambda im:im[y0:y1,x0:x1]
   reference=crop(base['linear-isolated']-images['linear-isolated']);candidate=crop(base['world-linear']-images['world-linear'])
   error=float(np.max(np.abs(candidate-reference)));row=dict(effect=effect,region=region,bounds=[x0,y0,x1,y1],maximumResponseError=error,referenceMaximumResponse=float(np.max(np.abs(reference))),candidateMaximumResponse=float(np.max(np.abs(candidate))),passed=error<=.008);report['comparisons'].append(row)
  (run/'native-effects-summary.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
 for name,digest in spec['inputs'].items():assert sha(project/name)==digest,name
 assert sha(player)==report['programSha256']
 report['status']='measured';report['responseBudgetPassed']=all(row['passed'] for row in report['comparisons'])
finally:
 if report['status']=='running':report['status']='failed'
 (run/'native-effects-summary.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in report.items() if k not in {'cases','comparisons'}}))
