import renderdoc as rd,json,sys,os,traceback,hashlib
from pathlib import Path
r=Path(r'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261029-052336-native-hierarchical-reflection-52b603')
e=r.parents[2];source=r.parent/'20261022-201845-endfield-capture-954719/Endfield-games-child_frame3534.rdc'
sys.path.insert(0,str(e/'tools'))
from native_render.renderdoc_replay import _variable,_descriptor,sampler_binding
cap=None;controller=None;report=dict(status='starting',source=str(source),events=[])
try:
    cap=rd.OpenCaptureFile();assert cap.OpenFile(str(source),'',None)==rd.ResultCode.Succeeded
    status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded
    textures={str(t.resourceId):dict(width=t.width,height=t.height,mips=t.mips,format=t.format.Name()) for t in controller.GetTextures()}
    for event in [8644,8662,8671,8691,8702]:
        controller.SetFrameEvent(event,True);pipe=controller.GetPipelineState();stage=rd.ShaderStage.Compute
        shader=pipe.GetShader(stage);reflection=pipe.GetShaderReflection(stage);assert reflection
        data=bytes(reflection.rawBytes);stem='event-{}'.format(event)
        (r/(stem+'.dxbc')).write_bytes(data)
        asm=controller.DisassembleShader(pipe.GetComputePipelineObject(),reflection,'')
        (r/(stem+'.asm')).write_text(asm,encoding='utf-8')
        blocks=[]
        for index,block in enumerate(reflection.constantBlocks):
            bound=pipe.GetConstantBlock(stage,index,0).descriptor
            variables=controller.GetCBufferVariableContents(pipe.GetComputePipelineObject(),shader,stage,pipe.GetShaderEntryPoint(stage),index,bound.resource,bound.byteOffset,bound.byteSize)
            blocks.append(dict(name=block.name,bytes=block.byteSize,variables=[_variable(v) for v in variables]))
        record=dict(event=event,shader=str(shader),sha256=hashlib.sha256(data).hexdigest(),blocks=blocks)
        for name,items in [('inputs',pipe.GetReadOnlyResources(stage)),('outputs',pipe.GetReadWriteResources(stage))]:
            record[name]=[]
            for item in items:
                value=_descriptor(item);value['texture']=textures.get(value['resource']);record[name].append(value)
        record['samplers']=[sampler_binding(item) for item in pipe.GetSamplers(stage)]
        (r/(stem+'.json')).write_text(json.dumps(record,indent=2))
        report['events'].append(dict(event=event,shader=str(shader),sha256=record['sha256'],constantBytes=[b['bytes'] for b in blocks]))
    report['status']='passed'
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
    if controller:controller.Shutdown()
    if cap:cap.Shutdown()
    (r/'receipt.json').write_text(json.dumps(report,indent=2))
os._exit(0)
