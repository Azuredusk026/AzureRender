"""Run sequential G11 follow-up checks and the complete Release gate."""
import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/g11'
def run(name,args,msvc=False):
    command=([str(root/'tools/msvc_env.bat')]+args) if msvc else args
    with (work/(name+'.log')).open('w',encoding='utf-8') as log:
        result=subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-5000:])
    print(name+' passed',flush=True)
for config in ('Debug','Release'):
    build='build/ninja-msvc-'+config.lower()
    run('abi-table-'+config.lower()+'-build',['cmake','--build',build,'--target','AzureManagedModuleTests'],True)
    run('abi-table-'+config.lower(),['ctest','--test-dir',build,'-C',config,'-R','^AzureEngine.ManagedModule.','--output-on-failure'])
run('release-gate',['cmake','-DBUILD_DIR=build/ninja-msvc-release','-DCONFIG=Release','-P','tools/run_release_gate.cmake'],True)
(work/'release-last-test.log').write_bytes((root/'build/ninja-msvc-release/Testing/Temporary/LastTest.log').read_bytes())
run('release-discovery',['ctest','--test-dir','build/ninja-msvc-release','-C','Release','--show-only=json-v1'])
run('post-gates',['python',str(work/'post_gates.py')])
print('Complete G11 gates passed',flush=True)
