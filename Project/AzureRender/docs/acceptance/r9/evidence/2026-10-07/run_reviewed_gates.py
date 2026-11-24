"""Archive initial gates and run both configurations against final R9 inputs."""
from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r9'
saved=work/'pre-review-gates';saved.mkdir(exist_ok=True)
for name in ['debug-build.log','debug-tests.log','release-build.log','release-gate.log']:
 shutil.copy2(work/name,saved/name)
for name in ['test.log','result.json']:
 source=root/'build/ninja-msvc-release/release-gate'/name
 if source.is_file():shutil.copy2(source,saved/('release-'+name))
result=subprocess.run(['python',str(work/'run_gates.py')],cwd=root)
if result.returncode:raise RuntimeError('Reviewed source full regression failed')
print('Both reviewed source gates passed',flush=True)
