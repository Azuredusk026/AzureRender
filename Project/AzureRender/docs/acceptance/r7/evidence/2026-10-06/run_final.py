"""Run the frozen final R7 sequence; any failed gate stops progression."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r7'
for name in ('run_gates.py','post_gates.py','close_phase.py'):
    prefix=[] if name=='run_gates.py' else [str(root/'tools/msvc_env.bat')]
    result=subprocess.run(prefix+['python',str(work/name)],cwd=root)
    if result.returncode:raise RuntimeError(name+' failed')
print('All R7 final gates passed; staging review and commit remain',flush=True)
