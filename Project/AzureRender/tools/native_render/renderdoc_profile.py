"""Read per-event replay GPU duration; compatible with RenderDoc Python 3.8."""
import json
import math
import hashlib
from pathlib import Path
from .renderdoc_capture import fingerprint


def summarize(rows):
    if not rows:raise ValueError('No measured GPU events')
    seen=set();groups={}
    for row in rows:
        event=row['event'];seconds=row['seconds']
        if event in seen or not math.isfinite(seconds) or seconds<0:
            raise ValueError('Duplicate event or invalid GPU duration')
        seen.add(event);key=(row['kind'],row['shader'])
        group=groups.setdefault(key,dict(kind=key[0],shader=key[1],events=0,milliseconds=0.0))
        group['events']+=1;group['milliseconds']+=seconds*1000
    values=sorted(groups.values(),key=lambda g:g['milliseconds'],reverse=True)
    return dict(events=len(rows),sumEventMs=sum(g['milliseconds'] for g in values),groups=values,
                scope='Replay event durations; independent of runtime frame latency')


def select_shader_events(rows,limit):
    if type(limit) is not int or not 1<=limit<=512:raise ValueError('Shader inspection limit must be within 1..512')
    summarize(rows)
    return {row['event'] for row in sorted(rows,key=lambda r:r['seconds'],reverse=True)[:limit]}


def profile_capture(rd,capture_path,output,shader_event_limit=64):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    capture=rd.OpenCaptureFile();controller=None
    try:
        if capture.OpenFile(str(capture_path),'',None)!=rd.ResultCode.Succeeded:
            raise RuntimeError('Cannot open selected capture')
        status,controller=capture.OpenCapture(rd.ReplayOptions(),None)
        if status!=rd.ResultCode.Succeeded:raise RuntimeError('Capture replay unavailable')
        counter=rd.GPUCounter.EventGPUDuration
        description=controller.DescribeCounter(counter)
        structured=controller.GetStructuredFile();actions={}
        def walk(items):
            for action in items:
                actions[action.eventId]=action
                walk(action.children)
        walk(controller.GetRootActions());rows=[];shaders={}
        for measurement in controller.FetchCounters([counter]):
            action=actions.get(measurement.eventId)
            if action is None:raise ValueError('Counter references an absent action')
            kind='dispatch' if action.flags&rd.ActionFlags.Dispatch else 'draw' if action.flags&rd.ActionFlags.Drawcall else 'other'
            rows.append(dict(event=action.eventId,name=action.GetName(structured),seconds=measurement.value.d,kind=kind,shader='uninspected' if kind!='other' else 'none'))
        selected=select_shader_events(rows,shader_event_limit)
        (output/'counter-events.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
        for row in rows:
            kind=row['kind'];action=actions[row['event']]
            if kind!='other' and row['event'] in selected:
                controller.SetFrameEvent(action.eventId,True);pipeline=controller.GetPipelineState()
                stage=rd.ShaderStage.Compute if kind=='dispatch' else rd.ShaderStage.Pixel
                shader=str(pipeline.GetShader(stage))
                if shader not in shaders:
                    reflection=pipeline.GetShaderReflection(stage)
                    shaders[shader]=dict(stage=str(stage),constantBlockBytes=[b.byteSize for b in reflection.constantBlocks] if reflection else [],
                        sourceFiles=[f.filename for f in reflection.debugInfo.files] if reflection else [],
                        sha256=hashlib.sha256(bytes(reflection.rawBytes)).hexdigest() if reflection else None)
                row['shader']=shader
        result=summarize(rows)
        result.update(status='profiled',source=fingerprint(capture_path),counter=dict(name=description.name,unit=str(description.unit)),shaders=shaders,measurements=rows,
            shaderInspectionLimit=shader_event_limit,shaderInspectedEvents=sum(row['shader'] not in ('none','uninspected') for row in rows))
        (output/'profile.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        return result
    finally:
        if controller:controller.Shutdown()
        capture.Shutdown()
