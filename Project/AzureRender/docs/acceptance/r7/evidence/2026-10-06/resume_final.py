from pathlib import Path
import re
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r7'
assert re.search(r'100% tests passed, 0 tests failed out of 117\b',(work/'debug-tests.log').read_text(encoding='utf-8'))
for config in ('Debug','Release'):
    subprocess.run(['python','tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-'+config.lower(),'--config',config],cwd=root,check=True)
with (work/'release-gate.log').open('w',encoding='utf-8') as log:
    subprocess.run([str(root/'tools/msvc_env.bat'),'cmake','-DBUILD_DIR=build/ninja-msvc-release','-DCONFIG=Release','-P','tools/run_release_gate.cmake'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
print('release-gate passed',flush=True)
for name in ('post_gates.py','close_phase.py'):
    subprocess.run([str(root/'tools/msvc_env.bat'),'python',str(work/name)],cwd=root,check=True)
print('All R7 final gates passed; staging review and commit remain',flush=True)
