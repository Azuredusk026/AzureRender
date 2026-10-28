import renderdoc as rd
from pathlib import Path
import subprocess,json,os,time,traceback,array,math,hashlib
r=Path(r'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261029-025624-main-reflection-history-3693f3/engine-probe')
e=r.parents[3];control=None;cap=None;controller=None;report=dict(status='starting',messages=[],outputs=[])
try:
    player=e/'build/ninja-msvc-release/AzurePlayer.exe'
    args=['--project',str(r/'project/project.azureproject'),'--width','320','--height','240','--capture-dir',str(r/'frames'),
          '--capture-frames','300','--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','reflection']
    (r/'command.json').write_text(json.dumps([str(player)]+args,indent=2))
    layer=e/'build/runs/20261021-172156-native-render-evidence-623c52/renderdoc/portable-1.46/RenderDoc_1.46_64'
    values={'VK_LAYER_PATH':str(layer),'VK_INSTANCE_LAYERS':'VK_LAYER_RENDERDOC_Capture','ENABLE_VULKAN_RENDERDOC_CAPTURE':'1'}
    previous={k:os.environ.get(k) for k in values}
    try:
        os.environ.update(values)
        result=rd.ExecuteAndInject(str(player),str(player.parent),subprocess.list2cmdline(args),[],str(r/'capture'),rd.CaptureOptions(),False)
    finally:
        for k,v in previous.items():
            if v is None:os.environ.pop(k,None)
            else:os.environ[k]=v
    assert result.ident,str(result.result)
    control=rd.CreateTargetControl('',result.ident,'AzureRender history probe',True);assert control
    report['pid']=control.GetPID();deadline=time.monotonic()+90;triggered=False
    while control.Connected() and time.monotonic()<deadline:
        if not triggered and (r/'frames/frame_000010.png').exists():control.TriggerCapture(1);triggered=True;report['triggered']=True
        message=control.ReceiveMessage(None)
        if str(message.type) not in report['messages']:report['messages'].append(str(message.type))
        time.sleep(.01)
    assert not control.Connected(),'Player did not finish its bounded capture'
    control.Shutdown();control=None
    captures=sorted(r.glob('*.rdc'));assert captures,'No engine capture was produced'
    capture=captures[-1];report['capture']=dict(path=str(capture),bytes=capture.stat().st_size,sha256=hashlib.sha256(capture.read_bytes()).hexdigest())
    cap=rd.OpenCaptureFile();assert cap.OpenFile(str(capture),'',None)==rd.ResultCode.Succeeded
    status,controller=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded
    textures={str(t.resourceId):t for t in controller.GetTextures()}
    def walk(actions):
        for action in actions:
            yield action
            yield from walk(action.children)
    for action in walk(controller.GetRootActions()):
        if not action.flags&rd.ActionFlags.Dispatch:continue
        controller.SetFrameEvent(action.eventId,True);pipe=controller.GetPipelineState();shader=pipe.GetShaderReflection(rd.ShaderStage.Compute)
        if not shader or not any(block.byteSize==48 for block in shader.constantBlocks):continue
        views=[item.descriptor.resource for item in pipe.GetReadWriteResources(rd.ShaderStage.Compute)]
        images=[textures[str(v)] for v in views if str(v) in textures]
        if len(images)!=2 or sorted(t.format.compCount for t in images)!=[1,4] or not all(t.format.compByteWidth==4 for t in images):continue
        weight=next(t for t in images if t.format.compCount==1)
        data=bytes(controller.GetTextureData(weight.resourceId,rd.Subresource(0,0,0)));values=array.array('f',data)
        assert len(values)==weight.width*weight.height and all(math.isfinite(v) and 0<=v<=1 for v in values)
        region=[values[y*weight.width+x] for y in range(weight.height*5//6,weight.height*23//24) for x in range(weight.width*3//8,weight.width*5//8)]
        record=dict(event=action.eventId,resource=str(weight.resourceId),width=weight.width,height=weight.height,
            positivePixels=sum(v>0 for v in values),minimum=min(values),maximum=max(values),centralFloorAcceptedRatio=sum(v>.5 for v in region)/len(region),sha256=hashlib.sha256(data).hexdigest())
        report['outputs'].append(record);(r/'acceptance.bin').write_bytes(data)
        assert record['positivePixels']>100 and record['centralFloorAcceptedRatio']>=.8,record
    assert report['outputs'],'The engine history reprojection dispatch was not identified'
    report['status']='passed'
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
    if controller:controller.Shutdown()
    if cap:cap.Shutdown()
    if control:control.Shutdown()
    (r/'receipt.json').write_text(json.dumps(report,indent=2))
os._exit(0)
