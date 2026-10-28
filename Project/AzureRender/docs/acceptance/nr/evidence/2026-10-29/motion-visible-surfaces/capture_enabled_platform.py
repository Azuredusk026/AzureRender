import renderdoc as rd,json,subprocess,os,time,traceback,hashlib,struct,sys
from pathlib import Path
r=Path(r'D:/Assigment/2609/FYP/Project/AzureRender/build/runs/20261029-060530-temporal-cloth-edge-74920a');e=r.parents[2]
p=r/'enabled-platform-probe';p.mkdir(exist_ok=True)
control=None;cap=None;c=None;report=dict(status='starting',inputs=[])
try:
    player=e/'build/ninja-msvc-release/AzurePlayer.exe'
    command=json.loads((r/'enabled-platform/command.json').read_text());command[0]=str(player)
    command[command.index('--capture-dir')+1]=str(p/'frames');command[command.index('--capture-frames')+1]='160'
    (p/'command.json').write_text(json.dumps(command,indent=2))
    layer=e/'build/runs/20261021-172156-native-render-evidence-623c52/renderdoc/portable-1.46/RenderDoc_1.46_64'
    values={'VK_LAYER_PATH':str(layer),'VK_INSTANCE_LAYERS':'VK_LAYER_RENDERDOC_Capture','ENABLE_VULKAN_RENDERDOC_CAPTURE':'1'};previous={k:os.environ.get(k) for k in values}
    try:
        os.environ.update(values);result=rd.ExecuteAndInject(str(player),str(player.parent),subprocess.list2cmdline(command[1:]),[],str(p/'capture'),rd.CaptureOptions(),False)
    finally:
        for k,v in previous.items():
            if v is None:os.environ.pop(k,None)
            else:os.environ[k]=v
    assert result.ident,str(result.result)
    control=rd.CreateTargetControl('',result.ident,'AzureRender temporal coverage',False);assert control
    deadline=time.monotonic()+180;triggered=False
    while control.Connected() and time.monotonic()<deadline:
        if not triggered and (p/'frames/frame_000022.png').exists():control.TriggerCapture(1);triggered=True
        control.ReceiveMessage(None);time.sleep(.005)
    assert triggered and not control.Connected()
    control.Shutdown();control=None
    captures=list(p.glob('*.rdc'));assert len(captures)==1
    cap=rd.OpenCaptureFile();assert cap.OpenFile(str(captures[0]),'',None)==rd.ResultCode.Succeeded
    status,c=cap.OpenCapture(rd.ReplayOptions(),None);assert status==rd.ResultCode.Succeeded
    expected=hashlib.sha256((player.parent/'shaders/temporal_resolve.frag.spv').read_bytes()).hexdigest()
    textures={str(t.resourceId):t for t in c.GetTextures()}
    def walk(actions):
        for action in actions:
            yield action
            yield from walk(action.children)
    for action in walk(c.GetRootActions()):
        if not action.flags&rd.ActionFlags.Drawcall:continue
        c.SetFrameEvent(action.eventId,True);pipe=c.GetPipelineState();reflection=pipe.GetShaderReflection(rd.ShaderStage.Pixel)
        if not reflection or hashlib.sha256(bytes(reflection.rawBytes)).hexdigest()!=expected:continue
        report['event']=action.eventId
        for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
            resource=used.descriptor.resource;texture=textures.get(str(resource))
            if not texture:continue
            raw=bytes(c.GetTextureData(resource,rd.Subresource(0,0,0)))
            width=texture.width;components=texture.format.compCount;size=texture.format.compByteWidth
            code='e' if size==2 else 'f';samples=[]
            for y in [99,100,101]:
                for x in [48,63,64,65,80]:
                    samples.append(dict(x=x,y=y,value=list(struct.unpack_from('<'+code*components,raw,(y*width+x)*components*size))))
            report['inputs'].append(dict(binding=used.access.index,resource=str(resource),format=texture.format.Name(),samples=samples))
        bound=pipe.GetConstantBlock(rd.ShaderStage.Pixel,0,0).descriptor
        data=bytes(c.GetBufferData(bound.resource,bound.byteOffset,bound.byteSize));report['constants']=list(struct.unpack('<'+'f'*(len(data)//4),data))
    assert report['inputs'];report['status']='passed'
except Exception:report.update(status='failed',error=traceback.format_exc())
finally:
    if c:c.Shutdown()
    if cap:cap.Shutdown()
    if control:control.Shutdown()
    (p/'receipt.json').write_text(json.dumps(report,indent=2))
os._exit(0)
