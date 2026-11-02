import json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');r=json.loads((run/'rdc-parallel-frame4-layer/receipt.json').read_text());t=json.loads((run/'parallel-tail-inspection.json').read_text())
cap=rd.OpenCaptureFile();controller=None;report=dict(status='running',stage='opening-file',samples=[])
def progress(): (run/'replay-pixel-stability.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
progress()
try:
 assert cap.OpenFile(r['capture']['path'],'',None)==rd.ResultCode.Succeeded
 report['stage']='opening-replay';progress();status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status)
 textures={str(v.resourceId):v for v in controller.GetTextures()};main=textures['ResourceId::176'].resourceId
 for repeat in range(3):
  report['stage']='replay-%d'%repeat;progress();controller.SetFrameEvent(t['draws'][-1]['event'],True)
  report['stage']='pick-%d'%repeat;progress();report['samples'].append(list(controller.PickPixel(main,927,3,rd.Subresource(),rd.CompType.Typeless).floatValue));progress()
 report.update(status='measured',stage='shutdown');progress()
except Exception:report.update(status='failed',error=traceback.format_exc());progress()
finally:
 if controller:controller.Shutdown()
 cap.Shutdown()
sys.exit(0)
