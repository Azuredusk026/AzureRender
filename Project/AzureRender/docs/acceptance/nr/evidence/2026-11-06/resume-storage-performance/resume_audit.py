from pathlib import Path
import ctypes
import json
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
out = Path(__file__).resolve().parent
sys.path.insert(0, str(root/'tools'))
from gpu_environment import snapshot, startup_errors
from artifact_budget import check

manifest = json.loads((root/'docs/development/artifact-manifest.json').read_text(encoding='utf-8'))
registered = {row['path'] for row in manifest['preservedRuns']}
pins = {p.parent.relative_to(root/'build').as_posix() for p in (root/'build/runs').glob('*/.keep')}
pin_audit = dict(pins=len(pins), registered=len(registered), unregistered=sorted(pins-registered),
                 missingMarkers=sorted(registered-pins))
(out/'pin-audit.json').write_text(json.dumps(pin_audit, indent=2), encoding='utf-8')

class HostPower(ctypes.Structure):
    _fields_ = [('ac',ctypes.c_ubyte),('batteryFlag',ctypes.c_ubyte),('batteryPercent',ctypes.c_ubyte),
                ('saver',ctypes.c_ubyte),('life',ctypes.c_ulong),('fullLife',ctypes.c_ulong)]

rows=[]
start=time.monotonic()
for index in range(3):
    if index: time.sleep(5)
    row=snapshot()
    power=HostPower()
    assert ctypes.windll.kernel32.GetSystemPowerStatus(ctypes.byref(power))
    row.update(seconds=time.monotonic()-start,hostPower=dict(ac=power.ac,saver=power.saver,batteryPercent=power.batteryPercent))
    rows.append(row)
apps=subprocess.run(['nvidia-smi','--query-compute-apps=pid,process_name,used_memory','--format=csv'],capture_output=True,text=True,check=True).stdout
result=dict(sourceBase='23015e7', observations=rows, startupErrors=startup_errors(rows),
            powerStable=all(r['hostPower']['ac']==1 and r['hostPower']['saver']==0 and r['powerLimitWatts']==80 for r in rows),
            gpuApps=apps, normalBudget=check(root/'build'),
            visualAcceptance='failed-user-review',formalPerformanceCollected=False)
(out/'environment.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(dict(startupErrors=result['startupErrors'],powerStable=result['powerStable'],pins=pin_audit,apps=apps)))
