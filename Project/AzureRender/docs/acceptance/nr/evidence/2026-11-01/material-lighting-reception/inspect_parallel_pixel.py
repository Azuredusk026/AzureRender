import hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');receipt=json.loads((run/'rdc-parallel-frame4-layer/receipt.json').read_text());tail=json.loads((run/'parallel-tail-inspection.json').read_text());cap=rd.OpenCaptureFile();controller=None;report=dict(status='running',pixel=[927,3],rows=[])
try:
 assert cap.OpenFile(receipt['capture']['path'],'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status);textures={str(t.resourceId):t for t in controller.GetTextures()};name=tail['draws'][0]['targets'][0]['resource'];texture=textures[name];report['target']=dict(resource=name,width=texture.width,height=texture.height,format=texture.format.Name());controller.SetFrameEvent(tail['draws'][-1]['event'],True)
 modifications=controller.PixelHistory(texture.resourceId,927,3,rd.Subresource(),rd.CompType.Typeless)
 for m in modifications:
  row=dict(event=m.eventId,primitive=m.primitiveID,pre=list(m.preMod.col.floatValue),post=list(m.postMod.col.floatValue),shader=list(m.shaderOut.col.floatValue));report['rows'].append(row)
  for field in ['shaderDiscarded','depthTestFailed','stencilTestFailed','backfaceCulled','unboundPS','directShaderWrite']:row[field]=bool(getattr(m,field,False))
  controller.SetFrameEvent(m.eventId,True);p=controller.GetPipelineState();s=p.GetShaderReflection(rd.ShaderStage.Pixel)
  if s:row['pixelShaderSha256']=hashlib.sha256(bytes(s.rawBytes)).hexdigest()
  row['inputs']=[dict(slot=s.readOnlyResources[u.access.index].fixedBindNumber,name=s.readOnlyResources[u.access.index].name,resource=str(u.descriptor.resource)) for u in p.GetReadOnlyResources(rd.ShaderStage.Pixel) if s and u.access.index<len(s.readOnlyResources)]
 report['status']='inspected-pixel-history'
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
 if controller:controller.Shutdown()
 cap.Shutdown();(run/'parallel-pixel-history.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
sys.exit(0)
