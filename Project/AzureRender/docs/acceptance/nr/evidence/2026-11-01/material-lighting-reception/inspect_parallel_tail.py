import hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');receipt=json.loads((run/'rdc-parallel-frame4-layer/receipt.json').read_text());build=json.loads((run/'release-build.json').read_text());shader_names={v:k for k,v in build['shaders'].items()};cap=rd.OpenCaptureFile();controller=None;report=dict(status='running',draws=[])
try:
 assert cap.OpenFile(receipt['capture']['path'],'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status);textures={t.resourceId:t for t in controller.GetTextures()};actions=[]
 def visit(nodes):
  for a in nodes:
   if a.flags&rd.ActionFlags.Drawcall:actions.append(a)
   visit(a.children)
 visit(controller.GetRootActions());report['drawCount']=len(actions)
 for action in actions[-32:]:
  controller.SetFrameEvent(action.eventId,True);pipe=controller.GetPipelineState();shader=pipe.GetShaderReflection(rd.ShaderStage.Pixel)
  if not shader:continue
  digest=hashlib.sha256(bytes(shader.rawBytes)).hexdigest();row=dict(event=action.eventId,indices=action.numIndices,instances=action.numInstances,pixelShaderSha256=digest,program=shader_names.get(digest),targets=[],inputs=[]);report['draws'].append(row)
  for target in pipe.GetOutputTargets():
   rid=target.resource
   if rid in textures:
    t=textures[rid];row['targets'].append(dict(resource=str(rid),width=t.width,height=t.height,format=t.format.Name()))
  for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
   if used.access.index>=len(shader.readOnlyResources):continue
   resource=shader.readOnlyResources[used.access.index];rid=used.descriptor.resource;item=dict(slot=resource.fixedBindNumber,name=resource.name,resource=str(rid));row['inputs'].append(item)
   if rid in textures:
    t=textures[rid];item.update(width=t.width,height=t.height,format=t.format.Name())
  if row['program'] in ['temporal_display_resolve.frag.spv','temporal_resolve.frag.spv']:
   report['temporalEvent']=action.eventId
   for item in row['inputs']:
    if item['slot'] not in [0,1,2,3,4,5,7,8]:continue
    rid=next(used.descriptor.resource for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel) if used.access.index<len(shader.readOnlyResources) and shader.readOnlyResources[used.access.index].fixedBindNumber==item['slot'])
    if rid in textures:item['pixel927x3']=list(controller.PickPixel(rid,927,3,rd.Subresource(),rd.CompType.Typeless).floatValue)
 report['status']='inspected-tail'
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
 if controller:controller.Shutdown()
 cap.Shutdown();(run/'parallel-tail-inspection.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
sys.exit(0)
