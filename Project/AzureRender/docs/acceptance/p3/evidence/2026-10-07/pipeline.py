import os,subprocess,json,sys
from pathlib import Path
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/p3';os.environ['VCPKG_ROOT']='C:/Users/23587/Tools/vcpkg'
def run(name,args,msvc=False):
 with (work/(name+'.log')).open('w',encoding='utf-8') as log:
  r=subprocess.run(([str(root/'tools/msvc_env.bat')]+args) if msvc else args,cwd=root,stdout=log,stderr=subprocess.STDOUT)
 if r.returncode:raise RuntimeError(name+': '+(work/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-7000:])
 print(name+' passed',flush=True)
for config in ('Debug','Release'):
 run(config.lower()+'-build',['python','tools/build_provenance.py','build','--build-dir','build/ninja-msvc-'+config.lower(),'--config',config],True)
run('debug-tests',['ctest','--test-dir','build/ninja-msvc-debug','-C','Debug','--output-on-failure'])
(work/'debug-last-test.log').write_bytes((root/'build/ninja-msvc-debug/Testing/Temporary/LastTest.log').read_bytes())
for config in ('Debug','Release'):
 run(config.lower()+'-discovery',['ctest','--test-dir','build/ninja-msvc-'+config.lower(),'-C',config,'--show-only=json-v1'])
run('release-gate',['cmake','-DBUILD_DIR=build/ninja-msvc-release','-DCONFIG=Release','-P','tools/run_release_gate.cmake'],True)
(work/'release-last-test.log').write_bytes((root/'build/ninja-msvc-release/Testing/Temporary/LastTest.log').read_bytes())
run('post-gates',['python',str(work/'post_gates.py')])
run('modules-off',['python',str(work/'check_modules_off.py')])
run('installed-contracts',['python','tools/write_engine_contracts.py','--verify','--output','build/ninja-msvc-release/release-gate/install-moved/share/AzureRender/contracts'])
run('authoring-publish',['python','tools/test_editor_playable.py','--executable','build/ninja-msvc-release/release-gate/install-moved/bin/AzureRender.exe','--install','build/ninja-msvc-release/release-gate/install-moved','--output',str(work/'authoring')])
run('coverage',['python','tools/verify_evolution_coverage.py','--manifest','docs/plans/engine-evolution-manifest.json','--evidence',str(work/'coverage'),'--build-dir','build/ninja-msvc-release','--configuration','Release','--historical-policy','committed-content'])
(work/'gate-status.json').write_text(json.dumps({'status':'passed','longRun':'pending'},indent=2)+'\n')
print('P3 primary gates passed; real long run required',flush=True)
