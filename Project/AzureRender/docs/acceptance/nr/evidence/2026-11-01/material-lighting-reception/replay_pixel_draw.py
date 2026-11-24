import json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');root=run.parents[2];sys.path.insert(0,str(root/'tools/native_render'));from renderdoc_capture import fingerprint;from renderdoc_replay import replay_capture
capture=json.loads((run/'rdc-parallel-frame4-layer/receipt.json').read_text())['capture']['path'];out=run/'pixel-draw-details';out.mkdir(exist_ok=False);request=dict(source=fingerprint(capture),output=str(out),events=[26483]);receipt=dict(format='RenderDocEvidence/1',operation='replay',source=request['source'],status='running',artifacts=[],tool_version=rd.GetVersionString())
try:replay_capture(rd,request,receipt)
except Exception:receipt.update(status='failed',error=traceback.format_exc())
finally:(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
sys.exit(0)
