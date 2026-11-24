import hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7')
receipt=json.loads((run/'rdc-parallel-frame4-layer/receipt.json').read_text())
tail=json.loads((run/'parallel-tail-inspection.json').read_text())
cap=rd.OpenCaptureFile(); controller=None; report=dict(status='running',samples=[])
try:
 assert cap.OpenFile(receipt['capture']['path'],'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status)
 textures={str(t.resourceId):t for t in controller.GetTextures()}
 end=tail['draws'][-1]['event']; main=textures['ResourceId::176'].resourceId
 for repeat in range(3):
  controller.SetFrameEvent(0,True);controller.SetFrameEvent(end,True)
  pipe=controller.GetPipelineState(); final=pipe.GetOutputTargets()[0].resource
  row=dict(repeat=repeat,targets=[]);report['samples'].append(row)
  for name,rid in [('main',main),('final',final)]:
   data=bytes(controller.GetTextureData(rid,rd.Subresource()))
   row['targets'].append(dict(name=name,resource=str(rid),sha256=hashlib.sha256(data).hexdigest(),pixel=list(controller.PickPixel(rid,927,3,rd.Subresource(),rd.CompType.Typeless).floatValue)))
 report['status']='measured-replay-stability'
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
 if controller:controller.Shutdown()
 cap.Shutdown();(run/'replay-stability.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
sys.exit(0)
