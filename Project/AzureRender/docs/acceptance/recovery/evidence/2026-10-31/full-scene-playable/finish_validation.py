from pathlib import Path
import json,subprocess,sys,time
r=Path(__file__).resolve().parent;deadline=time.monotonic()+1200
while not (r/'moving-debug/summary.json').exists():
 assert time.monotonic()<deadline,'Debug replay did not produce completion evidence';time.sleep(.25)
summary=json.loads((r/'moving-debug/summary.json').read_text());assert summary['status']=='captured-behavior-review-pending',summary
steps=[['verify_behavior.py','--name','moving-debug'],['review_route.py','--name','moving-debug'],['capture_route.py','--name','railing-green','--frames','240','--actions',str(r/'railing-input.json')],['verify_railing.py'],['capture_fixed.py'],['verify_inputs.py']]
for i,step in enumerate(steps):
 with (r/f'finish-step-{i}.log').open('wb') as log:
  process=subprocess.run([sys.executable,'-X','utf8',str(r/step[0]),*step[1:]],stdout=log,stderr=log)
 assert process.returncode==0,dict(step=step,log=f'finish-step-{i}.log',exitCode=process.returncode)
 print('Passed: '+step[0],flush=True)
(r/'joint-validation-status.json').write_bytes((json.dumps(dict(status='passed',steps=steps),indent=2)+'\n').encode())
