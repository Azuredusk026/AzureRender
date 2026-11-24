import gzip,hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');root=run.parents[2];sys.path.insert(0,str(root/'tools/native_render'));from renderdoc_replay import _variable
out=run/'recording-pair-inputs';out.mkdir(exist_ok=False);report=dict(status='running',captures=[])
def progress(): (run/'recording-pair-inputs.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
def save(folder,name,data):
 data=bytes(data);p=out/folder/(name+'.gz');p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(gzip.compress(data,mtime=0));return dict(path=p.relative_to(out).as_posix(),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
def variables(controller,pipe,stage,reflection):
 rows=[]
 for index,block in enumerate(reflection.constantBlocks):
  bound=pipe.GetConstantBlock(stage,index,0).descriptor
  values=controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),pipe.GetShader(stage),stage,pipe.GetShaderEntryPoint(stage),index,bound.resource,bound.byteOffset,bound.byteSize)
  rows.append(dict(name=block.name,variables=[_variable(v) for v in values]))
 return rows
def visit_variable(items):
 for item in items:
  yield item
  yield from visit_variable(item['members'])
try:
 for kind in ['parallel','serial']:
  cap=rd.OpenCaptureFile();controller=None;row=dict(kind=kind,stage='open',buffers=[],textures=[],constants=[]);report['captures'].append(row);progress()
  try:
   receipt=json.loads((run/('rdc-'+kind+'-frame10-layer/receipt.json')).read_text());assert receipt['status']=='captured'
   assert cap.OpenFile(receipt['capture']['path'],'',None)==rd.ResultCode.Succeeded
   status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status)
   actions=[]
   def walk(nodes):
    for a in nodes:
     if a.flags&rd.ActionFlags.Drawcall:actions.append(a)
     walk(a.children)
   walk(controller.GetRootActions());candidates=[a for a in actions[-400:] if a.numIndices==2160 and a.numInstances==1];row['candidateEvents']=[a.eventId for a in candidates];progress()
   chosen=None
   for a in reversed(candidates):
    controller.SetFrameEvent(a.eventId,True);pipe=controller.GetPipelineState();reflection=pipe.GetShaderReflection(rd.ShaderStage.Pixel)
    if not reflection:continue
    blocks=variables(controller,pipe,rd.ShaderStage.Pixel,reflection)
    padding=[v['uint32'][0] for b in blocks for v in visit_variable(b['variables']) if v['name']=='materialPadding']
    if padding==[1406]:chosen=a;break
   assert chosen is not None,'Captured glyph material 703 not found'
   row.update(event=chosen.eventId,stage='buffers');progress();buffer_info={b.resourceId:b for b in controller.GetBuffers()};texture_info={t.resourceId:t for t in controller.GetTextures()}
   for stage in [rd.ShaderStage.Vertex,rd.ShaderStage.Pixel]:
    reflection=pipe.GetShaderReflection(stage);row['constants'].append(dict(stage=str(stage),shaderSha256=hashlib.sha256(bytes(reflection.rawBytes)).hexdigest(),blocks=variables(controller,pipe,stage,reflection)))
    for used in pipe.GetReadWriteResources(stage):
     d=used.descriptor
     if d.resource not in buffer_info:continue
     name='%s-storage-%d-%d'%(str(stage),used.access.index,used.access.arrayElement);data=controller.GetBufferData(d.resource,d.byteOffset,d.byteSize)
     row['buffers'].append(dict(name=name,offset=d.byteOffset,**save(kind,name,data)));progress()
   for i,b in enumerate(pipe.GetVBuffers()):
    if b.resourceId not in buffer_info:continue
    name='vertex-%d'%i;row['buffers'].append(dict(name=name,offset=b.byteOffset,**save(kind,name,controller.GetBufferData(b.resourceId,b.byteOffset,buffer_info[b.resourceId].length-b.byteOffset))));progress()
   b=pipe.GetIBuffer();assert b.resourceId in buffer_info;name='index';row['buffers'].append(dict(name=name,offset=b.byteOffset,**save(kind,name,controller.GetBufferData(b.resourceId,b.byteOffset,buffer_info[b.resourceId].length-b.byteOffset))));progress()
   row['stage']='textures';progress();reflection=pipe.GetShaderReflection(rd.ShaderStage.Pixel)
   for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
    if used.access.index>=len(reflection.readOnlyResources):continue
    declaration=reflection.readOnlyResources[used.access.index];d=used.descriptor
    if declaration.name!='overlaySceneDepth' and not(declaration.name=='textureArray' and 6329<=used.access.arrayElement<=6335):continue
    t=texture_info[d.resource];name='%s-%d'%(declaration.name,used.access.arrayElement);row['textures'].append(dict(name=name,width=t.width,height=t.height,format=t.format.Name(),**save(kind,name,controller.GetTextureData(d.resource,rd.Subresource()))));progress()
   target=pipe.GetOutputTargets()[0].resource;row['stage']='output';progress();row['pixel911x4']=list(controller.PickPixel(target,911,4,rd.Subresource(),rd.CompType.Typeless).floatValue);row['mainHdr']=save(kind,'main-hdr',controller.GetTextureData(target,rd.Subresource()));row['stage']='complete';progress()
  finally:
   if controller:controller.Shutdown()
   cap.Shutdown()
 report['status']='measured-recording-pair-inputs';progress()
except Exception:report.update(status='failed',error=traceback.format_exc());progress()
sys.exit(0)
