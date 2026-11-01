"""Run through the installed RenderDoc Python runtime, with GPU work serialized."""
import hashlib,json,sys,traceback
from pathlib import Path
import renderdoc as rd

run=Path(globals().get('__file__','D:/Projects/FYP/Project/AzureRender/build/runs/20261101-143507-native-cloth-variants-d877ef/export_native_bindings.py')).resolve().parent;root=run.parents[2];capture=root/'build/runs/20261022-201845-endfield-capture-954719/Endfield-games-child_frame10401.rdc';out=run/'transparent-bindings';out.mkdir(exist_ok=False);cap=rd.OpenCaptureFile();controller=None;report=dict(status='running',capture=str(capture),draws=[]);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();saved={}
def save(name,data):
 p=out/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(bytes(data));return dict(path=name,bytes=p.stat().st_size,sha256=sha(p))
try:
 report['captureSha256']=sha(capture);assert cap.OpenFile(str(capture),'',None)==rd.ResultCode.Succeeded
 status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded,str(status)
 textures={t.resourceId:t for t in controller.GetTextures()}
 for event in [1383]:
  controller.SetFrameEvent(event,True);pipe=controller.GetPipelineState();shader=pipe.GetShaderReflection(rd.ShaderStage.Pixel);row=dict(event=event,shader=save(f'e{event}/ps.dxbc',shader.rawBytes),textures=[],constantBuffers=[]);report['draws'].append(row)
  save(f'e{event}/ps.asm',controller.DisassembleShader(pipe.GetGraphicsPipelineObject(),shader,'').encode())
  for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
   if used.access.index>=len(shader.readOnlyResources):continue
   slot=shader.readOnlyResources[used.access.index].fixedBindNumber;rid=used.descriptor.resource
   if rid not in textures:continue
   t=textures[rid];binding=dict(slot=slot,resource=str(rid),width=t.width,height=t.height,mips=t.mips,format=t.format.Name());row['textures'].append(binding)
   if slot not in [14,15,19,20,21]:continue
   if rid not in saved:
    name='textures/'+str(rid).replace('::','-')+'.dds';settings=rd.TextureSave();settings.resourceId=rid;settings.destType=rd.FileType.DDS;settings.mip=-1;p=out/name;p.parent.mkdir(parents=True,exist_ok=True);result=controller.SaveTexture(settings,str(p));assert result==rd.ResultCode.Succeeded,str(result);saved[rid]=dict(path=name,bytes=p.stat().st_size,sha256=sha(p))
   binding['export']=saved[rid]
  for index,block in enumerate(shader.constantBlocks):
   bound=pipe.GetConstantBlock(rd.ShaderStage.Pixel,index,0).descriptor
   if bound.byteSize==0:continue
   data=controller.GetBufferData(bound.resource,bound.byteOffset,bound.byteSize);row['constantBuffers'].append(dict(index=index,name=block.name,resource=str(bound.resource),offset=bound.byteOffset,size=bound.byteSize,**save(f'e{event}/cb{index}.bin',data)))
 report['status']='exported-transparent-bindings'
except Exception:
 report['status']='failed';report['error']=traceback.format_exc()
finally:
 if controller:controller.Shutdown()
 cap.Shutdown();(run/'transparent-bindings.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
sys.exit(0)
