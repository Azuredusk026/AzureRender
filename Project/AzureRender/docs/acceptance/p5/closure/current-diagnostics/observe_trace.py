from pathlib import Path
import sys,json,time
sys.path.insert(0,str(Path('tools').resolve()))
from collect_memory_residency import ResidentObserver
from run_playable_performance import memory_snapshot
pid=int(sys.argv[1]);out=Path(sys.argv[2]);rows=[];errors=[]
observer=ResidentObserver(pid);start=time.monotonic()
try:
    for _ in range(600):
        try:
            snap=observer.snapshot();snap['seconds']=time.monotonic()-start
            snap['counters']=memory_snapshot(pid);rows.append(snap)
        except OSError as e:
            errors.append(str(e));break
        out.write_text(json.dumps(dict(pid=pid,diagnosticOnly=True,snapshots=rows,errors=errors),indent=2)+'\n')
        time.sleep(1)
finally:
    observer.close()
    out.write_text(json.dumps(dict(pid=pid,diagnosticOnly=True,snapshots=rows,errors=errors),indent=2)+'\n')
