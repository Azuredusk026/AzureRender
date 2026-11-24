import hashlib,json,os,re,subprocess,time
from pathlib import Path
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;player=root/'build/ninja-msvc-release/AzurePlayer.exe';project=run/'project/playable.azureproject';out=run/'native-entry';out.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
command=[str(player),'--project',str(project),'--width','960','--height','540','--capture-frames','3','--fixed-frame-step','--capture-fps','60','--capture-dir',str(out/'frames'),'--runtime-report',str(out/'runtime.json')]
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
record=dict(status='running',command=command,programSha256=sha(player),qaFlagsProvided=False);start=time.perf_counter()
try:
 with (out/'player.log').open('wb') as log:result=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
 record.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0
 assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(out/'player.log').read_text(errors='replace'),re.I)
 rows=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()]
 assert not any(row['level']=='error' or row['subsystem']=='validation' and row['level']=='warning' for row in rows)
 assert any(row['message']=='Allocator after unload: buffers=0 images=0' for row in rows)
 manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==3 and not manifest['qaHarnessEnabled']
 reference=json.loads((run/'route-release/frames/capture_manifest.json').read_text())
 for key in ['nprLighting','gradeSettings','shadowSettings','faceSdfSettings','bloomSettings','antiAliasing','materialDisplayTransforms','materialRadianceScales']:
  assert manifest[key]==reference[key],key
 images=[]
 for i in range(3):
  name='frame_'+str(i).zfill(6)+'.png';candidate=out/'frames'/name;reference=run/'route-release/frames'/name
  images.append(dict(frame=i,sha256=sha(candidate),referenceSha256=sha(reference),exact=Image.open(candidate).tobytes()==Image.open(reference).tobytes(),fileExact=candidate.read_bytes()==reference.read_bytes()))
 record.update(images=images,status='captured',pixelExact=all(row['exact'] for row in images));assert sha(player)==record['programSha256']
finally:
 if record['status']=='running':record['status']='failed'
 (out/'summary.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in record.items() if k not in {'command','images'}}))
