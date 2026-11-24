import renderdoc as rd,json,os,traceback,hashlib
from pathlib import Path
r=Path(r'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261029-052336-native-hierarchical-reflection-52b603')
request=json.loads((r/'mirror-request.json').read_text());label=request['label'];cap=None;c=None;original=None;replacement=None
report=dict(status='starting',outputs=[],label=label)
try:
    cap=rd.OpenCaptureFile();assert cap.OpenFile(str(r.parent/'20261022-201845-endfield-capture-954719/Endfield-games-child_frame3534.rdc'),'',None)==rd.ResultCode.Succeeded
    status,c=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded
    c.SetFrameEvent(8702,True);original=c.GetPipelineState().GetShader(rd.ShaderStage.Compute)
    textures={str(t.resourceId):t for t in c.GetTextures()};references={}
    for resource in [15861,16501,15781]:
        tex=textures['ResourceId::{}'.format(resource)];raw=bytes(c.GetTextureData(tex.resourceId,rd.Subresource(0,0,0)))
        references[resource]=raw;(r/'reference-{}.bin'.format(resource)).write_bytes(raw)
    source=(r/request['source']).read_bytes();report['sourceSha256']=hashlib.sha256(source).hexdigest()
    replacement,errors=c.BuildTargetShader('main',rd.ShaderEncoding.HLSL,source,rd.ShaderCompileFlags(),rd.ShaderStage.Compute)
    report['compiler']=str(errors);assert replacement!=rd.ResourceId.Null(),errors
    c.ReplaceResource(original,replacement);c.SetFrameEvent(8702,True)
    pipeline=c.GetPipelineState();reflection=pipeline.GetShaderReflection(rd.ShaderStage.Compute)
    (r/('{}-compiled.asm'.format(label))).write_text(c.DisassembleShader(pipeline.GetComputePipelineObject(),reflection,''),encoding='utf-8')
    report['compiledBlocks']=[dict(name=b.name,bytes=b.byteSize) for b in reflection.constantBlocks]
    for resource,reference in references.items():
        tex=textures['ResourceId::{}'.format(resource)];raw=bytes(c.GetTextureData(tex.resourceId,rd.Subresource(0,0,0)))
        assert len(raw)==len(reference)
        (r/'{}-{}.bin'.format(label,resource)).write_bytes(raw)
        report['outputs'].append(dict(resource=resource,bytes=len(raw),differentBytes=sum(a!=b for a,b in zip(raw,reference)),referenceSha256=hashlib.sha256(reference).hexdigest(),outputSha256=hashlib.sha256(raw).hexdigest()))
    assert all(x['differentBytes']==0 for x in report['outputs']),report['outputs']
    report['status']='passed'
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
    if c and original and replacement and replacement!=rd.ResourceId.Null():c.RemoveReplacement(original);c.FreeTargetResource(replacement)
    if c:c.Shutdown()
    if cap:cap.Shutdown()
    (r/('{}-mirror.json'.format(label))).write_text(json.dumps(report,indent=2))
os._exit(0)
