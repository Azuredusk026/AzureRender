import json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');capture=json.loads((run/'rdc-parallel-frame9-layer/receipt.json').read_text());post=json.loads((run/'recording-post-pair9.json').read_text());source=next(t for t in post['captures'][0]['passes'][0]['textures'] if t['name']=='sceneColorTexture')['resource'];cap=rd.OpenCaptureFile();controller=None;report=dict(status='running',samples=[])
def progress():(run/'frame9-composition.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
progress()
try:
 assert cap.OpenFile(capture['capture']['path'],'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status);textures={str(t.resourceId):t for t in controller.GetTextures()};target=textures[source].resourceId
 for event in [26379,26380,26457,26458,26472,26473,26482,26483]:
  controller.SetFrameEvent(event,True);report['samples'].append(dict(event=event,pixels=[dict(x=x,y=4,rgba=list(controller.PickPixel(target,x,4,rd.Subresource(),rd.CompType.Typeless).floatValue)) for x in [910,911,912]]));progress()
 report['status']='measured-frame9-composition';progress()
except Exception:report.update(status='failed',error=traceback.format_exc());progress()
finally:
 if controller:controller.Shutdown()
 cap.Shutdown()
sys.exit(0)
