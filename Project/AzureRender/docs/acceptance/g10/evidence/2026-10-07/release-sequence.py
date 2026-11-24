"""Resume release gates after repairing the generated toolchain cache."""
from pathlib import Path
import os
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/g10'
env=dict(os.environ,VCPKG_ROOT='C:/Users/23587/Tools/vcpkg')
for key in ('HTTP_PROXY','HTTPS_PROXY','ALL_PROXY'):env.pop(key,None)
steps=[
 ('debug-provenance-retained',['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-debug','--config','Debug']),
 ('release-build',[str(root/'tools/msvc_env.bat'),'python','tools/build_provenance.py','build','--build-dir','build/ninja-msvc-release','--config','Release']),
 ('release-gate',[str(root/'tools/msvc_env.bat'),'cmake','-DBUILD_DIR=build/ninja-msvc-release','-DCONFIG=Release','-P','tools/run_release_gate.cmake']),
 ('post-gates',['python',str(work/'post_gates.py')]),
]
for name,command in steps:
 with (work/(name+'.log')).open('w',encoding='utf-8') as output:
  result=subprocess.run(command,cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT)
 if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-6500:])
 print(name+' passed',flush=True)
