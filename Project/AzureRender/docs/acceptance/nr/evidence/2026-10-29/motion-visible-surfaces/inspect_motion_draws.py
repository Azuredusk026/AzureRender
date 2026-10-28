import renderdoc as rd,json,hashlib,sys,os,traceback
from pathlib import Path
r=Path(r'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261029-060530-temporal-cloth-edge-74920a');e=r.parents[2];p=r/'coverage-probe'
sys.path.insert(0,str(e/'tools'));from native_render.renderdoc_replay import _variable
cap=None;c=None;report=dict(status='starting',draws=[])
try:
    receipt=json.loads((p/'receipt.json').read_text());motion=next(x['resource'] for x in receipt['inputs'] if x['binding']==4)
    shaders={hashlib.sha256(f.read_bytes()).hexdigest():f.name for f in (e/'build/ninja-msvc-release/shaders').glob('*.spv')}
    cap=rd.OpenCaptureFile();assert cap.OpenFile(str(next(p.glob('*.rdc'))),'',None)==rd.ResultCode.Succeeded
    status,c=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded
    def walk(items):
        for action in items:
            yield action
            yield from walk(action.children)
    for action in walk(c.GetRootActions()):
        if not action.flags&rd.ActionFlags.Drawcall or action.eventId>=receipt['event']:continue
        c.SetFrameEvent(action.eventId,True);pipe=c.GetPipelineState()
        if motion not in [str(t.resource) for t in pipe.GetOutputTargets()]:continue
        blocks=[]
        for stage in [rd.ShaderStage.Vertex,rd.ShaderStage.Pixel]:
            reflection=pipe.GetShaderReflection(stage)
            for index,block in enumerate(reflection.constantBlocks):
                bound=pipe.GetConstantBlock(stage,index,0).descriptor
                variables=c.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),pipe.GetShader(stage),stage,pipe.GetShaderEntryPoint(stage),index,bound.resource,bound.byteOffset,bound.byteSize)
                blocks.append(dict(stage=str(stage),name=block.name,variables=[_variable(v) for v in variables]))
        vertex=pipe.GetShaderReflection(rd.ShaderStage.Vertex)
        report['draws'].append(dict(event=action.eventId,vertex=shaders.get(hashlib.sha256(bytes(vertex.rawBytes)).hexdigest()),blocks=blocks))
    report['status']='passed'
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
    if c:c.Shutdown()
    if cap:cap.Shutdown()
    (p/'motion-draws.json').write_text(json.dumps(report,indent=2))
os._exit(0)
