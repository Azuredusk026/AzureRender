import hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');root=run.parents[2];sys.path.insert(0,str(root/'tools/native_render'));from renderdoc_replay import _variable
capture=json.loads((run/'rdc-parallel-frame9-layer/receipt.json').read_text());post=json.loads((run/'recording-post-pair9.json').read_text());source=next(t for t in post['captures'][0]['passes'][0]['textures'] if t['name']=='sceneColorTexture')['resource'];cap=rd.OpenCaptureFile();controller=None;report=dict(status='running',pixels=[])
def progress():(run/'frame9-pixel-history.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
def visit(items):
 for v in items:
  yield v
  yield from visit(v['members'])
progress()
try:
 assert cap.OpenFile(capture['capture']['path'],'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status);textures={str(t.resourceId):t for t in controller.GetTextures()};target=textures[source].resourceId;draws=set()
 def walk(nodes):
  for a in nodes:
   if a.flags&rd.ActionFlags.Drawcall:draws.add(a.eventId)
   walk(a.children)
 walk(controller.GetRootActions())
 for x,y in [(911,4),(912,4),(910,4)]:
  controller.SetFrameEvent(post['captures'][0]['passes'][0]['event'],True);row=dict(pixel=[x,y],modifications=[]);report['pixels'].append(row);progress()
  for m in controller.PixelHistory(target,x,y,rd.Subresource(),rd.CompType.Typeless):
   item=dict(event=m.eventId,draw=m.eventId in draws,primitive=m.primitiveID,pre=list(m.preMod.col.floatValue),post=list(m.postMod.col.floatValue),shader=list(m.shaderOut.col.floatValue));row['modifications'].append(item)
   for name in ['shaderDiscarded','depthTestFailed','stencilTestFailed','backfaceCulled','unboundPS']:item[name]=bool(getattr(m,name,False))
   if item['draw']:
    controller.SetFrameEvent(m.eventId,True);pipe=controller.GetPipelineState();reflection=pipe.GetShaderReflection(rd.ShaderStage.Pixel)
    if reflection:
     item['shaderSha256']=hashlib.sha256(bytes(reflection.rawBytes)).hexdigest()
     for index,block in enumerate(reflection.constantBlocks):
      if block.name!='material':continue
      bound=pipe.GetConstantBlock(rd.ShaderStage.Pixel,index,0).descriptor;values=controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),pipe.GetShader(rd.ShaderStage.Pixel),rd.ShaderStage.Pixel,pipe.GetShaderEntryPoint(rd.ShaderStage.Pixel),index,bound.resource,bound.byteOffset,bound.byteSize)
      item['material']={v['name']:v['uint32'] for v in visit([_variable(v) for v in values]) if v['name'] in ['materialPadding','materialClass','materialFeatures','textureBase']}
   progress()
 report['status']='measured-frame9-history';progress()
except Exception:report.update(status='failed',error=traceback.format_exc());progress()
finally:
 if controller:controller.Shutdown()
 cap.Shutdown()
sys.exit(0)
