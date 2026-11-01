import hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-143507-native-cloth-variants-d877ef');root=run.parents[2]
source=root/'build/runs/20261022-201845-endfield-capture-954719/Endfield-games-child_frame10401.rdc'
out=run/'variant-targets';out.mkdir(exist_ok=False)
report=dict(status='running',capture=str(source),draws=[]);cap=rd.OpenCaptureFile();controller=None
def save(name,data):
 p=out/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(bytes(data))
 return dict(path=name,bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
def progress(): (run/'variant-targets.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
try:
 assert cap.OpenFile(str(source),'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status)
 textures={t.resourceId:t for t in controller.GetTextures()}
 for event in [889,910,928,1383]:
  row=dict(event=event,outputs=[]);report['draws'].append(row);progress()
  for eid in [event-1,event]:
   controller.SetFrameEvent(eid,True);pipe=controller.GetPipelineState();rid=pipe.GetOutputTargets()[0].resource;t=textures[rid]
   row['outputs'].append(dict(event=eid,resource=str(rid),width=t.width,height=t.height,format=t.format.Name(),**save('e%d/target-%d.bin'%(event,eid),controller.GetTextureData(rid,rd.Subresource()))));progress()
  shader=pipe.GetShaderReflection(rd.ShaderStage.Pixel)
  row['shader']=save('e%d/ps.dxbc'%event,shader.rawBytes)
  save('e%d/ps.asm'%event,controller.DisassembleShader(pipe.GetGraphicsPipelineObject(),shader,'').encode())
  row['textures']=[];row['constants']=[]
  for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
   if used.access.index>=len(shader.readOnlyResources):continue
   slot=shader.readOnlyResources[used.access.index].fixedBindNumber;rid=used.descriptor.resource
   if rid not in textures:continue
   t=textures[rid];binding=dict(slot=slot,resource=str(rid),width=t.width,height=t.height,mips=t.mips,format=t.format.Name());row['textures'].append(binding)
   if slot not in [11,12,16,17,18,19]:continue
   setting=rd.TextureSave();setting.resourceId=rid;setting.destType=rd.FileType.DDS;setting.mip=-1;p=out/('e%d/t%d.dds'%(event,slot));p.parent.mkdir(exist_ok=True)
   assert controller.SaveTexture(setting,str(p))==rd.ResultCode.Succeeded
   binding['export']=dict(path=p.relative_to(out).as_posix(),sha256=hashlib.sha256(p.read_bytes()).hexdigest())
  for index,block in enumerate(shader.constantBlocks):
   bound=pipe.GetConstantBlock(rd.ShaderStage.Pixel,index,0).descriptor
   if bound.byteSize:row['constants'].append(dict(index=index,name=block.name,**save('e%d/cb%d.bin'%(event,index),controller.GetBufferData(bound.resource,bound.byteOffset,bound.byteSize))))
  progress()
 report['status']='exported-variant-targets';progress()
except Exception:report.update(status='failed',error=traceback.format_exc());progress()
finally:
 if controller:controller.Shutdown()
 cap.Shutdown()
sys.exit(0)
