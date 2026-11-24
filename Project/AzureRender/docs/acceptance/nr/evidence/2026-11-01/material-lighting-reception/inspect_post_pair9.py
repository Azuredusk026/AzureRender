import gzip,hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');root=run.parents[2];sys.path.insert(0,str(root/'tools/native_render'));from renderdoc_replay import _variable,sampler_binding
out=run/'recording-post-pair9';out.mkdir(exist_ok=False);build=json.loads((run/'release-build.json').read_text());wanted={build['shaders'][n]:n for n in ['inner_outline.frag.spv','spatial_antialiasing.frag.spv']};report=dict(status='running',captures=[])
def progress():(run/'recording-post-pair9.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
def save(kind,name,data):
 data=bytes(data);p=out/kind/(name+'.gz');p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(gzip.compress(data,mtime=0));return dict(path=p.relative_to(out).as_posix(),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
try:
 for kind in ['parallel','serial']:
  cap=rd.OpenCaptureFile();controller=None;record=dict(kind=kind,stage='open',passes=[]);report['captures'].append(record);progress()
  try:
   receipt=json.loads((run/('rdc-'+kind+'-frame9-layer/receipt.json')).read_text());assert cap.OpenFile(receipt['capture']['path'],'',None)==rd.ResultCode.Succeeded
   status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status);actions=[]
   def walk(nodes):
    for a in nodes:
     if a.flags&rd.ActionFlags.Drawcall:actions.append(a)
     walk(a.children)
   walk(controller.GetRootActions());textures={t.resourceId:t for t in controller.GetTextures()}
   for action in actions[-3:]:
    controller.SetFrameEvent(action.eventId,True);pipe=controller.GetPipelineState();shader=pipe.GetShaderReflection(rd.ShaderStage.Pixel);digest=hashlib.sha256(bytes(shader.rawBytes)).hexdigest()
    if digest not in wanted:continue
    name=wanted[digest];row=dict(program=name,event=action.eventId,constants=[],textures=[],samplers=[sampler_binding(v) for v in pipe.GetSamplers(rd.ShaderStage.Pixel)]);record['passes'].append(row);record['stage']=name;progress()
    for index,block in enumerate(shader.constantBlocks):
     bound=pipe.GetConstantBlock(rd.ShaderStage.Pixel,index,0).descriptor;values=controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),pipe.GetShader(rd.ShaderStage.Pixel),rd.ShaderStage.Pixel,pipe.GetShaderEntryPoint(rd.ShaderStage.Pixel),index,bound.resource,bound.byteOffset,bound.byteSize)
     row['constants'].append(dict(name=block.name,variables=[_variable(v) for v in values]))
    seen={}
    for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
     if used.access.index>=len(shader.readOnlyResources):continue
     declaration=shader.readOnlyResources[used.access.index];d=used.descriptor
     if declaration.name not in ['normalTexture','depthTexture','sceneColorTexture','displayMaskTexture','colorGradingLutTexture','linearSource']:continue
     t=textures[d.resource];binding=dict(name=declaration.name,resource=str(d.resource),width=t.width,height=t.height,depth=t.depth,format=t.format.Name());row['textures'].append(binding);progress()
     if d.resource not in seen:seen[d.resource]=save(kind,name+'-'+declaration.name,controller.GetTextureData(d.resource,rd.Subresource()))
     binding.update(seen[d.resource]);progress()
    target=pipe.GetOutputTargets()[0].resource;t=textures[target];row['output']=dict(width=t.width,height=t.height,format=t.format.Name(),pixel911x4=list(controller.PickPixel(target,911,4,rd.Subresource(),rd.CompType.Typeless).floatValue),**save(kind,name+'-output',controller.GetTextureData(target,rd.Subresource())));progress()
   assert len(record['passes'])==2;record['stage']='complete';progress()
  finally:
   if controller:controller.Shutdown()
   cap.Shutdown()
 report['status']='measured-post-pair';progress()
except Exception:report.update(status='failed',error=traceback.format_exc());progress()
sys.exit(0)
