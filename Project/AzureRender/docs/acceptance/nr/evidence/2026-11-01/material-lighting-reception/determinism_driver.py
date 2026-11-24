import json,subprocess,sys,time
from pathlib import Path
run=Path(__file__).resolve().parent;deadline=time.monotonic()+5400
while True:
 try:
  s=json.loads((run/'resumed-regression-pipeline.json').read_text())['status']
  assert s!='failed','Dependent regression failed'
  if s=='passed':break
 except (FileNotFoundError,json.JSONDecodeError):pass
 assert time.monotonic()<deadline
 time.sleep(15)
for name in ['serial-a','serial-b']:
 print('START',name,flush=True)
 with (run/('determinism-'+name+'-driver.log')).open('wb') as log:subprocess.run([sys.executable,'-X','utf8',str(run/'capture_determinism.py'),'--name',name],stdout=log,stderr=log,timeout=750,check=True)
 print('DONE',name,flush=True)
a=json.loads((run/'determinism-serial-a.json').read_text());b=json.loads((run/'determinism-serial-b.json').read_text());different=[n for n,d in a['images'].items() if b['images'][n]!=d];report=dict(status='measured',serialDifferentFrames=different,defaultRepeatDifferentFrames=json.loads((run/'compatibility-repeat.json').read_text())['versusFirstDifferentFrames'],visualAcceptance='failed-user-review');(run/'determinism-comparison.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report))
