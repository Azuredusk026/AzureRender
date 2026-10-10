from pathlib import Path
import json,subprocess
w=Path('build/p5-p6-closure-2026-10-10');old=w/'threaded-return-trace-owned';out=w/'threaded-return-trace-single-step';out.mkdir()
c=json.loads((old/'config.json').read_text());c['output']=str(out.resolve());(out/'config.json').write_text(json.dumps(c))
s=(old/'commands.lldb').read_text().replace('threaded-return-trace-owned','threaded-return-trace-single-step').replace('threaded-proofs-owned','threaded-proofs-single-step')
s='settings set target.process.run-all-threads false\nsettings set target.process.thread.single-thread-plan-timeout 0\n'+s
(out/'commands.lldb').write_text(s)
with (out/'lldb.log').open('w') as f:r=subprocess.run(['C:/Program Files/LLVM/bin/lldb.exe','-b','-s',str(out/'commands.lldb')],stdout=f,stderr=subprocess.STDOUT,timeout=300)
print(r.returncode)
