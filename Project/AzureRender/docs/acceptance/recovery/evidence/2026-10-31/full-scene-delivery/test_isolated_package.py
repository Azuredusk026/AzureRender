from pathlib import Path
import hashlib,json,os,re,shutil,struct,subprocess,sys,time,urllib.parse
from PIL import Image
import numpy as np
root=Path.cwd();r=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'));import build_game
publication=json.loads((r/'publication.json').read_text());assert publication['status']=='built-isolation-review-pending'
package=(r/'独立游戏 中文 空格').resolve();source=(r/'published-game').resolve();assert source.is_relative_to(r.resolve()) and package.is_relative_to(r.resolve()) and not package.exists();source.rename(package)
manifest=json.loads((package/'game_manifest.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert sha(package/'game_manifest.json')==publication['manifestSha256']
models=0;relative=[];embedded=0
for item in manifest['files']:
 p=package/item['path']
 if p.suffix.lower() not in ['.glb','.gltf']:continue
 models+=1
 if p.suffix.lower()=='.glb':
  with p.open('rb') as stream:
   header=stream.read(20);assert header[:4]==b'glTF';size,kind=struct.unpack_from('<II',header,12);assert kind==0x4e4f534a;data=json.loads(stream.read(size).decode('utf-8').rstrip('\0 '))
 else:data=json.loads(p.read_text(encoding='utf-8-sig'))
 for kind in ['buffers','images']:
  for entry in data.get(kind,[]):
   uri=entry.get('uri')
   if uri is None or uri.startswith('data:'):embedded+=1;continue
   parsed=urllib.parse.urlsplit(uri);assert not parsed.scheme and not parsed.netloc,dict(asset=item['path'],uri=uri)
   target=(p.parent/urllib.parse.unquote(uri)).resolve();assert target.is_relative_to(package) and target.is_file(),dict(asset=item['path'],uri=uri)
   relative.append(dict(asset=item['path'],uri=uri,target=target.relative_to(package).as_posix()))
(r/'package-reference-closure.json').write_bytes((json.dumps(dict(status='passed',models=models,embeddedBuffersOrImages=embedded,relativeReferences=relative,scope='Every distributed glTF buffer and image is embedded or resolves inside the moved package'),indent=2)+'\n').encode())
env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith(('AZURERENDER','AZURE_CHARACTER','PYTHON','DOTNET','VULKAN','VCPKG','VK_LAYER','VK_INSTANCE_LAYERS'))};env['PATH']=str(Path(env['SYSTEMROOT'])/'System32')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
inputs=(r/'移动后的完整场景').resolve();hidden_inputs=(r/'input-unavailable').resolve();engine=(r/'engine').resolve();hidden_engine=(r/'engine-unavailable').resolve()
for a,b in [(inputs,hidden_inputs),(engine,hidden_engine)]:assert a.is_relative_to(r.resolve()) and b.is_relative_to(r.resolve()) and a!=r.resolve() and not b.exists()
inputs.rename(hidden_inputs);engine.rename(hidden_engine);report=dict(status='running',package=str(package),entry=manifest['entry'],sourceUnavailable=True,engineUnavailable=True,isolatedPath=env['PATH'],cases=[])
def execute(name,extra,timeout=1800):
 out=r/name;out.mkdir();command=[str(package/manifest['entry']),'--no-error-dialog',*extra,'--runtime-report',str(out/'runtime.json')];start=time.perf_counter()
 with (out/'launcher.log').open('wb') as log:
  process=subprocess.Popen(command,cwd=package,env=env,startupinfo=startup,stdout=log,stderr=log)
  try:code=process.wait(timeout=timeout)
  except subprocess.TimeoutExpired:
   subprocess.run([str(Path(env['SYSTEMROOT'])/'System32/taskkill.exe'),'/PID',str(process.pid),'/T','/F'],capture_output=True);process.wait();raise
 shutil.copy2(package/'captures/game-startup.log',out/'player.log');row=dict(name=name,command=command,exitCode=code,elapsedSeconds=time.perf_counter()-start);report['cases'].append(row);assert code==0,row
 log=(out/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log and not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
 data=json.loads((out/'runtime.json').read_text());assert not data['scriptErrors'] and not data['presentationErrors']
 return out
try:
 out=execute('native-default',['--width','960','--height','540','--capture-frames','3','--capture-fps','60','--fixed-frame-step','--capture-dir',str(r/'native-default/frames')])
 frames=[]
 for i in range(3):
  reference=r/f'moved-reopen/frames/frame_{i:06d}.png';actual=out/f'frames/frame_{i:06d}.png';a=np.asarray(Image.open(reference).convert('RGBA'));b=np.asarray(Image.open(actual).convert('RGBA'));assert a.shape==b.shape;changed=int(np.count_nonzero(np.any(a!=b,axis=2)));frames.append(dict(frame=i,changedPixels=changed,referenceSha256=sha(reference),actualSha256=sha(actual)));assert changed==0,frames[-1]
 capture=json.loads((out/'frames/capture_manifest.json').read_text());assert Path(capture['asset']).resolve().is_relative_to(package)
 (out/'pixel-verification.json').write_bytes((json.dumps(dict(status='passed',frames=frames,qaFlagsProvided=False,scope='Native packaged entry defaults match the saved source project at the same 960x540 reference resolution'),indent=2)+'\n').encode())
 shutil.copy2(root/'build/runs/20261031-183314-full-scene-playable-validation-d5257e/input-route.json',r/'package-actions.json')
 out=execute('native-route',['--capture-frames','540','--capture-fps','60','--fixed-frame-step','--capture-dir',str(r/'native-route/frames'),'--game-actions',str(r/'package-actions.json')])
 assert Image.open(out/'frames/frame_000539.png').size==(1920,1080),'Package default quality resolution was not applied'
 shutil.copy2(root/'build/runs/20261031-183314-full-scene-playable-validation-d5257e/verify_behavior.py',r/'verify_package_behavior.py')
 result=subprocess.run([sys.executable,'-X','utf8',str(r/'verify_package_behavior.py'),'--name','native-route'],capture_output=True);(out/'behavior-verification.log').write_bytes(result.stdout+result.stderr);assert result.returncode==0
 build_game.verify_package(package)
 guide=package/'GAME-GUIDE.md';saved=guide.read_bytes();guide.write_bytes(saved+b'\nchanged\n')
 try:
  try:build_game.verify_package(package)
  except ValueError:pass
  else:raise AssertionError('Changed immutable guide was accepted')
 finally:guide.write_bytes(saved)
 report.update(status='passed',immutableHashesPassed=True,tamperRejected=True,referencePixelFrames=3,routeFrames=540,defaultResolution=[1920,1080],manifestSha256=sha(package/'game_manifest.json'))
finally:
 for original,hidden in [(inputs,hidden_inputs),(engine,hidden_engine)]:
  if hidden.exists():hidden.rename(original)
 if report['status']=='running':report['status']='failed'
 (r/'isolation-summary.json').write_bytes((json.dumps(report,ensure_ascii=False,indent=2)+'\n').encode());print(json.dumps(report,ensure_ascii=False,indent=2))
