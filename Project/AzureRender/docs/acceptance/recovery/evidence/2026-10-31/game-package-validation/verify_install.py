from pathlib import Path
import hashlib,json,os,subprocess
root=Path.cwd();r=Path(__file__).resolve().parent;engine=r/'engine';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();log=(r/'install-green.log').read_text(errors='replace');assert 'Installed isolated Python builder runtime' in log and 'Invalid escape sequence' not in log
files=[]
for name in ['AzurePlayer.exe','AzureRender.exe','AzureGameLauncher.exe']:
 a=root/'build/ninja-msvc-release'/name;b=engine/'bin'/name;assert sha(a)==sha(b);files.append(dict(path='bin/'+name,sha256=sha(b)))
tool=engine/'share/AzureRender/tools/build_game.py';assert sha(tool)==sha(root/'tools/build_game.py');files.append(dict(path=tool.relative_to(engine).as_posix(),sha256=sha(tool)))
env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith(('PYTHON','VULKAN','VCPKG','DOTNET','AZURERENDER'))};env['PATH']=str(Path(env['SYSTEMROOT'])/'System32')+';'+env['SYSTEMROOT'];python=engine/'share/AzureRender/tools/python/python.exe'
code='import sys,json,ssl,pathlib;root=pathlib.Path(sys.executable).parent.resolve();assert sys.flags.isolated==1;assert all(pathlib.Path(p).resolve().is_relative_to(root) for p in sys.path);print(json.dumps(dict(version=sys.version,executable=sys.executable,paths=sys.path,isolated=sys.flags.isolated)))'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
result=subprocess.run([str(python),'-I','-X','utf8','-c',code],env=env,capture_output=True,text=True,encoding='utf-8',startupinfo=startup,timeout=30);assert result.returncode==0,result.stdout+result.stderr
report=dict(status='passed',normalizedPythonInstallPath=True,files=files,python=json.loads(result.stdout),isolatedPath=env['PATH']);(r/'install-verification.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print('Install path, exact programs, builder source and isolated Python passed')
