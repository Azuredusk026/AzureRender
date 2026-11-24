from pathlib import Path
import sys,json,subprocess,hashlib,os
r=Path(__file__).resolve().parent;root=r.parents[2]
out=r/'public-screen-shadow-head-capture';out.mkdir()
player=root/'build/ninja-msvc-release/AzurePlayer.exe'
command=[str(player),'--project',str(r/'public-back-view-static/qualified.azureproject'),'--width','2560','--height','1440','--capture-frames','1','--capture-fps','24','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--disable-bindless','--disable-compute-skinning','--disable-parallel-recording','--capture-dir',str(out/'frames')]
(out/'command.json').write_text(json.dumps(command,indent=2))
digest=hashlib.sha256(player.read_bytes()).hexdigest()
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
with (out/'player.log').open('wb') as log:subprocess.run(command,cwd=out,check=True,startupinfo=si,stdout=log,stderr=log,timeout=900)
assert 'Allocator after unload: buffers=0 images=0' in (out/'player.log').read_text(errors='replace')
assert hashlib.sha256(player.read_bytes()).hexdigest()==digest
shaderFolder=Path(os.environ['AZURERENDER_RESOURCE_ROOT'])/'shaders' if os.environ.get('AZURERENDER_RESOURCE_ROOT') else root/'build/ninja-msvc-release/shaders'
(out/'summary.json').write_text(json.dumps(dict(status='captured',executableSha256=digest,resourceRoot=str(shaderFolder.parent),shaders={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(shaderFolder.glob('*.spv'))}),indent=2))
