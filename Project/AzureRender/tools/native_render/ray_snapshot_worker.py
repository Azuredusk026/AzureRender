"""Export source-capture inputs inside RenderDoc's embedded Python."""
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import traceback
import renderdoc as rd

request=json.loads(Path(os.environ['AZURE_RAY_SNAPSHOT_REQUEST']).read_text(encoding='utf-8-sig'))
output=Path(request['output'])
engine=Path(request['engine'])
sys.path.insert(0,str(engine/'tools'))
from native_render.renderdoc_replay import _descriptor, sampler_binding

source=json.loads((engine/'docs/acceptance/nr/evidence/2026-10-29/native-ray-source/event-8702.json').read_text(encoding='utf-8'))
reference=json.loads((engine/'docs/acceptance/nr/evidence/2026-10-31/ray-arithmetic-order/reference.json').read_text(encoding='utf-8'))
report=dict(status='failed',files=[])
capture=controller=None
sha=lambda data:hashlib.sha256(data).hexdigest()
roles=['tiles','previousColor','previousDepth','currentDepth','motion','surfaceMask','surfaceNormal']
bytes_per_texel={'R16G16B16A16_FLOAT':8,'R32_FLOAT':4,'R10G10B10A2_UNORM':4,'R8_UNORM':1,'R11G11B10_FLOAT':4,'R16G16_UNORM':4}
total_bytes=0

def publish(name,data):
    global total_bytes
    total_bytes+=len(data)
    assert total_bytes<=128*1024*1024,'Snapshot exceeds the owned byte budget'
    with (output/name).open('xb') as target:target.write(data)
    item=dict(file=name,bytes=len(data),sha256=sha(data))
    report['files'].append(item)
    return item

def flatten(actions):
    for action in actions:
        yield action
        for child in flatten(action.children):yield child

try:
    capture=rd.OpenCaptureFile()
    assert capture.OpenFile(request['capture'],'',None)==rd.ResultCode.Succeeded
    status,controller=capture.OpenCapture(rd.ReplayOptions(),None)
    assert status==rd.ResultCode.Succeeded
    resources={str(item.resourceId):item for item in controller.GetTextures()}
    resources.update({str(item.resourceId):item for item in controller.GetBuffers()})
    controller.SetFrameEvent(0,True)
    controller.SetFrameEvent(reference['event'],True)
    action=next(a for a in flatten(controller.GetRootActions()) if a.eventId==reference['event'])
    groups=list(action.dispatchDimension)
    assert groups==[9115,1,1],groups
    report['dispatch']=groups
    pipeline=controller.GetPipelineState()
    reflection=pipeline.GetShaderReflection(rd.ShaderStage.Compute)
    assert sha(bytes(reflection.rawBytes))==reference['originalShaderSha256']
    inputs={item.access.index:_descriptor(item) for item in pipeline.GetReadOnlyResources(rd.ShaderStage.Compute)}
    outputs={item.access.index:_descriptor(item) for item in pipeline.GetReadWriteResources(rd.ShaderStage.Compute)}
    assert set(inputs)==set(range(7)) and set(outputs)==set(range(3))
    for expected in source['inputs']:assert inputs[expected['shader_binding_index']]=={k:v for k,v in expected.items() if k!='texture'}
    for expected in source['outputs']:assert outputs[expected['shader_binding_index']]=={k:v for k,v in expected.items() if k!='texture'}
    sampler=[sampler_binding(item) for item in pipeline.GetSamplers(rd.ShaderStage.Compute)]
    assert sampler==source['samplers']
    constants=[]
    for index,block in enumerate(reflection.constantBlocks):
        descriptor=pipeline.GetConstantBlock(rd.ShaderStage.Compute,index,0).descriptor
        entry=dict(index=index,resource=str(descriptor.resource),offset=descriptor.byteOffset,bytes=block.byteSize)
        data=bytes(controller.GetBufferData(resources[entry['resource']].resourceId,entry['offset'],entry['bytes']))
        expected=b''.join(struct.pack('<I',v) for variable in source['blocks'][index]['variables'] for v in variable['uint32'])
        assert data==expected and len(data)==entry['bytes']
        entry.update(publish('constant-{}.bin'.format(index),data))
        constants.append((entry,data))
    assert [entry['bytes'] for entry,data in constants]==[1040,48,96]

    def texture_data(descriptor):
        texture=resources[descriptor['resource']]
        fmt=texture.format.Name()
        assert fmt==descriptor['format'] and fmt in bytes_per_texel
        assert descriptor['first_slice']==0 and descriptor['slice_count']==1
        result=bytearray();levels=[]
        for mip in range(descriptor['first_mip'],descriptor['first_mip']+descriptor['mip_count']):
            width=max(1,texture.width>>mip);height=max(1,texture.height>>mip)
            data=bytes(controller.GetTextureData(texture.resourceId,rd.Subresource(mip,0,0)))
            assert len(data)==width*height*bytes_per_texel[fmt]
            levels.append(dict(mip=mip,width=width,height=height,offset=len(result),bytes=len(data)))
            result.extend(data)
        return dict(format=fmt,levels=levels,width=levels[0]['width'],height=levels[0]['height'],texelBytes=bytes_per_texel[fmt]),bytes(result)

    def tile_data():
        descriptor=inputs[0]
        return bytes(controller.GetBufferData(resources[descriptor['resource']].resourceId,descriptor['byte_offset'],descriptor['byte_size']))

    def tile_set(data):
        assert len(data)==115200
        words=struct.unpack_from('<'+'I'*groups[0],data)
        assert len(set(words))==groups[0]
        assert all((word>>16)<160 and (word&65535)<90 for word in words)
        return sorted(words)

    after_tiles=tile_data()
    after_textures={index:texture_data(inputs[index]) for index in range(1,7)}
    native={}
    for row in reference['outputs']:
        descriptor=next(item for item in outputs.values() if item['resource']=='ResourceId::{}'.format(row['resource']))
        metadata,data=texture_data(descriptor)
        assert len(data)==row['bytes'] and sha(data)==row['sha256']
        native[row['resource']]=(metadata,data)

    # Capture the state before the consuming dispatch. Reading u2 after the
    # dispatch would substitute the result for the classification input.
    controller.SetFrameEvent(0,True)
    controller.SetFrameEvent(reference['event']-1,True)
    before_tiles=tile_data()
    active_tiles=tile_set(before_tiles)
    assert active_tiles==tile_set(after_tiles),'Active tile identities changed across replay'
    tile_record=publish('tiles.bin',before_tiles)
    tile_record.update(inputs[0]);tile_record.update(stride=4,activeCount=groups[0],activeSetSha256=sha(struct.pack('<'+'I'*len(active_tiles),*active_tiles)))
    texture_records=[]
    for index in range(1,7):
        metadata,data=texture_data(inputs[index])
        assert (metadata,data)==after_textures[index],'Sampled resource changed across the consuming dispatch'
        record=publish(roles[index]+'.bin',data);record.update(metadata);record.update(role=roles[index],binding=index,source=inputs[index])
        texture_records.append(record)
    for entry,data in constants:
        actual=bytes(controller.GetBufferData(resources[entry['resource']].resourceId,entry['offset'],entry['bytes']))
        assert actual==data,'Constant contents changed across the consuming dispatch'
    output_records=[]
    for index,role in enumerate(('color','footprint','confidence')):
        metadata,before=texture_data(outputs[index])
        resource=int(outputs[index]['resource'].split('::')[1])
        after_metadata,after=native[resource]
        assert metadata==after_metadata
        initial=publish(role+'-initial.bin',before)
        expected=publish(role+'-reference.bin',after)
        changed=sum(a!=b for a,b in zip(before,after))
        if role=='confidence':assert changed>0,'The pre-dispatch confidence input must differ from its output'
        output_records.append(dict(role=role,binding=index,source=outputs[index],initial=initial,reference=expected,changedBytes=changed,**metadata))
    usage={}
    for descriptor in list(inputs.values())+list(outputs.values()):
        key=descriptor['resource']
        usage[key]=[dict(event=use.eventId,usage=str(use.usage)) for use in controller.GetUsage(resources[key].resourceId) if use.eventId<=8702]
    report['usage']=usage
    controller.SetFrameEvent(0,True)
    controller.SetFrameEvent(reference['event'],True)
    for row in reference['outputs']:
        descriptor=next(item for item in outputs.values() if item['resource']=='ResourceId::{}'.format(row['resource']))
        assert sha(texture_data(descriptor)[1])==row['sha256']
    manifest=dict(schemaVersion=1,status='complete',captureSha256=reference['captureSha256'],event=reference['event'],preEvent=reference['event']-1,
        originalShaderSha256=reference['originalShaderSha256'],dispatch=groups,localSize=[8,8,1],constants=[entry for entry,data in constants],tiles=tile_record,textures=texture_records,outputs=output_records,samplers=sampler,
        validation=dict(sampledTexturesUnchanged=True,constantsUnchanged=True,activeTileSetsEqual=True,originalOutputsExact=True,originalRepeatedExact=True),bytes=total_bytes)
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    report.update(status='passed',bytes=total_bytes,originalRepeatedExact=True)
except Exception:
    report['error']=traceback.format_exc()
finally:
    if controller:controller.Shutdown()
    if capture:capture.Shutdown()
    (output/'snapshot-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
os._exit(0 if report['status']=='passed' else 1)
