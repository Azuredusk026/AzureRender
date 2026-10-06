"""Run frozen R8 builds and full regression, stopping on every failed gate."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r8'
steps=[
 ('debug-build',[str(root/'tools/msvc_env.bat'),'python','tools/build_provenance.py','build','--build-dir','build/ninja-msvc-debug','--config','Debug']),
 ('debug-tests',['ctest','--test-dir','build/ninja-msvc-debug','-C','Debug','--output-on-failure']),
 ('release-build',[str(root/'tools/msvc_env.bat'),'python','tools/build_provenance.py','build','--build-dir','build/ninja-msvc-release','--config','Release']),
 ('release-gate',[str(root/'tools/msvc_env.bat'),'cmake','-DBUILD_DIR=build/ninja-msvc-release','-DCONFIG=Release','-P','tools/run_release_gate.cmake']),
]
for name,command in steps:
 with (work/(name+'.log')).open('w',encoding='utf-8') as output:
  result=subprocess.run(command,cwd=root,stdout=output,stderr=subprocess.STDOUT)
 if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6500:])
 print(name+' passed',flush=True)
