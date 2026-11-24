"""RenderDoc embedded Python entry; reuse the repository's explicit capture worker."""
import hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');root=run.parents[2];sys.path.insert(0,str(root/'tools/native_render'));from renderdoc_worker import capture_target
out=run/'rdc-parallel-frame4';out.mkdir(exist_ok=False);working=out/'target';working.mkdir();player=root/'build/ninja-msvc-release/AzurePlayer.exe';build=json.loads((run/'release-build.json').read_text());assert hashlib.sha256(player.read_bytes()).hexdigest()==build['programs']['AzurePlayer']
request=dict(operation='capture',source=dict(path=str(player),bytes=player.stat().st_size,sha256=build['programs']['AzurePlayer']),output=str(out),working_directory=str(working),frame=4,timeout=900,arguments=['--project',str(run/'candidate-project/full-world-hidden.azureproject'),'--width','2560','--height','1440','--capture-frames','12','--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--capture-dir',str(working/'frames'),'--runtime-report',str(working/'runtime.json')]);(out/'request.json').write_bytes((json.dumps(request,indent=2)+'\n').encode());receipt=dict(format='RenderDocEvidence/1',operation='capture',source=request['source'],status='running',artifacts=[],tool_version=rd.GetVersionString())
try:capture_target(rd,request,receipt)
except Exception:receipt.update(status='failed',error=traceback.format_exc())
finally:(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
sys.exit(0)
