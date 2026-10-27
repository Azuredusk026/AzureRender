"""Validate public texture layers through actual Player draws and independent pixels."""
import argparse
import base64
import copy
import hashlib
import io
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import component, surface
from convert_material_semantics import complete_material_document


def linear(value):
    value=np.asarray(value,dtype=np.float64)/255
    return np.where(value<=.04045,value/12.92,((value+.055)/1.055)**2.4)


def png_uri(pixels):
    stream=io.BytesIO();Image.fromarray(np.asarray(pixels,dtype=np.uint8),'RGBA').save(stream,format='PNG')
    return 'data:image/png;base64,'+base64.b64encode(stream.getvalue()).decode()


def layer(role,texture):
    return dict(role=role,texture=texture,sampleMode='rgba',uv=dict(set=0,
        scaleOffset=[1,1,0,0],rotation=[1,0,0,1],speed=[0,0]))


def profile():
    return dict(schemaVersion=2,lighting='unlit',layers=[layer('base',0)],tint=[1,1,1,1],
        blendTint=[2,1,.5,.5],clock=dict(rate=1,offsetSeconds=0),
        softIntersection=dict(distance=1.5,bias=0),highlightExpansion=dict(threshold=1,gain=0),
        postExposureWeight=1,globalColorWeight=1)


def run(player,root,expected_validation='any',selected=None):
    root.mkdir(parents=True,exist_ok=False);assets=root/'assets';assets.mkdir()
    inputs=[player.resolve(),Path(__file__).resolve(),Path(__file__).with_name('create_visibility_fixture.py'),
        Path(__file__).with_name('convert_material_semantics.py'),*sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprint=lambda:{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    frozen=fingerprint();(root/'input-fingerprints.json').write_text(json.dumps(frozen,indent=2))
    surface(assets/'surface.gltf',double_sided=True);template=json.loads((assets/'surface.gltf').read_text())
    data=base64.b64decode(template['buffers'][0]['uri'].split(',')[1]);data+=struct.pack('<8f',0,1,1,1,1,0,0,0)
    template['buffers'][0]=dict(byteLength=len(data),uri='data:application/octet-stream;base64,'+base64.b64encode(data).decode())
    template['bufferViews'].append(dict(buffer=0,byteOffset=len(data)-32,byteLength=32))
    template['accessors'].append(dict(bufferView=3,componentType=5126,count=4,type='VEC2'))
    template['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0']=3
    rear=copy.deepcopy(template);rear['materials'][0]=dict(doubleSided=True,
        pbrMetallicRoughness=dict(baseColorFactor=[.07,.09,.11,1]),
        extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface'],surface=dict(schemaVersion=1,lighting='unlit'))))
    (assets/'rear.gltf').write_text(json.dumps(rear));project=root/'project.azureproject'
    project.write_text(json.dumps(dict(schemaVersion=1,id='public-texture-layers',name='Public texture layers',
        mounts=[dict(name='assets',path='assets')],startupScene='assets:/scene.azurelevel')))
    base_pixel=np.array([230,180,250,128],dtype=np.uint8);second_pixel=np.array([128,64,192,102],dtype=np.uint8)
    pattern=np.array([[[230,128,64,255],[64,230,128,255]],[[128,64,230,255],[192,192,192,255]]],dtype=np.uint8)
    records=[];metrics={};baseline={};tolerance=.008;clear_background=None

    def capture(name,kind,*,fixed=False,frames=1):
        nonlocal clear_background
        p=profile();model=copy.deepcopy(template);opacity=.4 if kind in ('node-opacity','coverage-clamp','coverage-soft','view-soft') else 1
        delta=.75 if kind in ('soft-half','node-opacity','diagonal','orthographic') else 2
        if kind=='soft-zero':delta=0
        if kind=='depth-occluded':delta=-.25
        if kind=='coverage-clamp':p['tint'][3]=3;delta=1.125
        if kind=='coverage-soft':p['tint'][3]=2;delta=.75
        if kind=='coverage-negative':p['tint'][3]=-1
        if kind.startswith('view-'):
            p['viewDepthFade']=dict(near=[2,4],far=[10,20])
            if kind=='view-far':p['viewDepthFade']=dict(near=[0,1],far=[2,4])
            if kind=='view-zero':p['viewDepthFade']=dict(near=[4,5],far=[10,20])
            if kind=='view-soft':p['tint'][3]=3;delta=.75
        pixels=base_pixel.reshape(1,1,4);second=second_pixel.reshape(1,1,4)
        colors=dict(schemaVersion=1,exposureGain=1,saturation=1,colorGain=[1,1,1])
        if kind in ('mask','blend','document-mask'):p['layers'].append(layer('mask' if kind=='document-mask' else kind,1))
        if kind=='view-mask':p['layers'].append(layer('mask',1))
        if kind=='red-alpha':p['layers'][0]['sampleMode']='red-as-alpha'
        if kind in ('highlight','node-opacity'):p['tint']=[1.3,1.3,1.3,1];p['highlightExpansion']['gain']=3
        if kind=='frame-color':colors.update(exposureGain=1.5,saturation=.25,colorGain=[.75,1,.5])
        if kind in ('repeat','clamp','independent-samplers'):
            pixels=pattern;second=pattern;p['layers'][0]['uv']['scaleOffset']=[0,0,1.25,.25]
            if kind=='independent-samplers':p['layers'].append(layer('mask',1));p['layers'][1]['uv']['scaleOffset']=[0,0,1.25,.25]
        if kind in ('scroll','clock-frozen','clock-reverse'):
            values=np.linspace(.05,.7,64);encoded=np.round(np.where(values<=.0031308,values*12.92,1.055*values**(1/2.4)-.055)*255).astype(np.uint8)
            pixels=np.empty((1,64,4),dtype=np.uint8);pixels[0,:,:3]=encoded[:,None];pixels[0,:,3]=255
            p['layers'][0]['uv']['speed']=[.125,0]
            if kind=='clock-frozen':p['clock']=dict(rate=0,offsetSeconds=.25)
            if kind=='clock-reverse':p['clock']=dict(rate=-1,offsetSeconds=.5)
        samplers=[dict(wrapS=33071 if kind=='clamp' else 10497,wrapT=10497,minFilter=9729,magFilter=9729),
            dict(wrapS=33071,wrapT=10497,minFilter=9729,magFilter=9729)]
        model['images']=[dict(uri=png_uri(pixels)),dict(uri=png_uri(second))]
        model['textures']=[dict(source=0,sampler=0),dict(source=0 if kind=='independent-samplers' else 1,sampler=1)]
        model['samplers']=samplers
        model['materials'][0]=dict(name='PublicTextureLayers',alphaMode='BLEND',doubleSided=True,extras=dict(azureSlotId='surface',azureRenderMaterial=
            dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface','unlit-texture-layers'],surface=p)))
        document_id=None
        if kind=='document-mask':
            image_ids=['11111111-1111-4111-8111-111111111111','22222222-2222-4222-8222-222222222222'];document_id='33333333-3333-4333-8333-333333333333'
            for index,(image,image_id) in enumerate(zip(model['images'],image_ids)):
                image_path=assets/f'document-{index}.png';image_path.write_bytes(base64.b64decode(image['uri'].split(',')[1]))
                Path(str(image_path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=image_id,settings={},dependencies=[])))
            document=complete_material_document(model['materials'][0],model,{str(i):x for i,x in enumerate(image_ids)})
            (assets/'layers.azurematerial').write_text(json.dumps(document))
            (assets/'layers.azurematerial.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=document_id,settings={},dependencies=image_ids)))
            # A distinct embedded source is a positive control for real slot binding.
            model['materials'][0]['extras']['azureRenderMaterial']=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface'],surface=dict(schemaVersion=1,lighting='unlit'))
            model['materials'][0]['pbrMetallicRoughness']=dict(baseColorFactor=[.8,.1,.05,1])
        model['extensionsUsed']=['AZURE_node_opacity'];model['nodes'][0]['extensions']={'AZURE_node_opacity':dict(schemaVersion=1,opacity=opacity)}
        (assets/'surface.gltf').write_text(json.dumps(model))
        yaw=15 if kind in ('diagonal','view-diagonal') else 0;camera_x=3*math.tan(math.radians(yaw))
        camera=component('azure.camera',dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=20,
            orthographic=kind in ('orthographic','view-orthographic'),orthographicHeight=2));camera['version']=2
        nodes=[dict(id='surface',resourceId='surface',components={'azure.transform':component('azure.transform',dict(translation=[0,0,0]))}),
            dict(id='camera',components={'azure.transform':component('azure.transform',dict(translation=[camera_x,0,3],rotation=[0,yaw,0])), 'azure.camera':camera})]
        if document_id:nodes[0]['materialSlots']={'surface':document_id}
        if kind=='clear-only':nodes=[node for node in nodes if node['id']=='camera']
        if kind not in ('empty-background','clear-only'):nodes.append(dict(id='rear',resourceId='rear',components={'azure.transform':component('azure.transform',dict(translation=[0,0,-delta],scale=[10,10,10]))}))
        settings=dict(schemaVersion=20,unlitEffects=colors,sceneLighting=True,skyIntensity=0,cameraNear=.1,cameraFar=20,
            shadowDistance=20,platform=False,background=False,silhouetteOutline=False,innerOutline=False,
            stylized=False,bloomEnabled=False,antiAliasing=0,exposure=0,toneMapping=False,contrast=1,saturation=1)
        scene=dict(schemaVersion=1,id=name,sceneType='character',resources=[dict(id='surface',asset='assets:/surface.gltf'),dict(id='rear',asset='assets:/rear.gltf')],nodes=nodes,lights=[],renderSettings=settings)
        (assets/'scene.azurelevel').write_text(json.dumps(scene));out=root/name;out.mkdir()
        command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128',
            '--capture-dir',str((out/'frames').resolve()),'--capture-frames',str(frames),'--capture-fps','4',
            '--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty',
            '--runtime-report',str((out/'runtime.json').resolve())]
        if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
        (out/'command.json').write_text(json.dumps(command,indent=2))
        result=subprocess.run(command,capture_output=True,encoding='utf8',errors='replace',timeout=120)
        log=result.stdout+result.stderr;(out/'player.log').write_text(log,encoding='utf-8')
        assert result.returncode==0,(name,'Player failed; see player.log')
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
        assert 'Allocator after unload: buffers=0 images=0' in log,name
        validation='Validation layer: enabled' in log
        if expected_validation!='any':assert validation==(expected_validation=='enabled'),name
        report=json.loads((out/'runtime.json').read_text());assert not report['scriptErrors'] and not report['presentationErrors'],name
        manifest=json.loads((out/'frames/capture_manifest.json').read_text())
        assert manifest['materialSurfaceProfileLayoutVersion']==15 and manifest['cameraUniformBytes']==688 and manifest['unlitFrameLayoutVersion']==1,name
        assert np.allclose(manifest['qaCameraPosition'],[camera_x,0,3],atol=1e-6),name
        images=[np.asarray(Image.open(out/f'frames/frame_{i:06d}.png').convert('RGB')) for i in range(frames)]
        for i,image in enumerate(images):
            if kind=='clear-only':
                assert np.all(image==image[64,64]),(name,'empty display control must be uniform')
                clear_background=linear(image[64,64]);expected=clear_background
            elif kind in ('scroll','clock-frozen','clock-reverse'):
                time=i/4;tau=p['clock']['rate']*time+p['clock']['offsetSeconds']
                u=.5+(2*(64.5/128)-1)*3*math.tan(math.radians(27.5))+.125*tau
                sample_coordinate=u*64-.5;first=math.floor(sample_coordinate);fraction=sample_coordinate-first
                expected=(1-fraction)*linear(pixels[0,first%64,:3])+fraction*linear(pixels[0,(first+1)%64,:3])
            else:
                if kind in ('repeat','independent-samplers'):sample_pixel=pattern[0,0]
                elif kind=='clamp':sample_pixel=pattern[0,1]
                else:sample_pixel=base_pixel
                expected=linear(sample_pixel[:3]);alpha=sample_pixel[3]/255
                if kind=='red-alpha':alpha=expected[0];expected=np.ones(3)
                expected=expected*np.array(p['tint'][:3]);alpha*=p['tint'][3]
                if kind in ('mask','document-mask','independent-samplers','view-mask'):
                    mask=pattern[0,1] if kind=='independent-samplers' else second_pixel
                    expected*=linear(mask[:3]);alpha*=mask[3]/255
                if kind=='blend':
                    weight=min(1,max(0,(alpha+second_pixel[3]/255)*.5));expected+=linear(second_pixel[:3])*np.array([2,1,.5])*weight
                if kind in ('highlight','node-opacity'):expected+=np.maximum(expected-1,0)*3
                expected=np.clip(expected*colors['exposureGain'],0,1000)
                luminance=np.dot(expected,[.212673,.715152,.072175]);expected=(luminance+colors['saturation']*(expected-luminance))*np.array(colors['colorGain'])
                soft=1 if kind=='empty-background' else min(1,max(0,delta/math.cos(math.radians(yaw))/1.5))
                fade=1
                if kind.startswith('view-'):
                    depth=3/math.cos(math.radians(yaw));ranges=p['viewDepthFade'];near=ranges['near'];far=ranges['far']
                    fade=min(1,max(0,(depth-near[0])/(near[1]-near[0])))*min(1,max(0,(far[1]-depth)/(far[1]-far[0])))
                alpha=np.clip(alpha*soft*fade,0,1)*opacity;receiver=clear_background if kind=='empty-background' else np.array([.07,.09,.11])
                expected=receiver if kind=='depth-occluded' else expected*alpha+receiver*(1-alpha)
            # The target is an eight-bit presentation image. HDR survives the
            # transparent blend, then the display attachment clips its range.
            expected=np.clip(expected,0,1)
            error=float(np.max(np.abs(linear(image[64,64])-expected)))
            metrics[f'{name}-{i}']=error
            assert error<=tolerance,(name,i,'independent linear pixel',error,expected.tolist(),image[64,64].tolist())
        if frames>1:
            if kind=='clock-frozen':assert all(np.array_equal(images[0],x) for x in images[1:]),name
            else:assert max(int(np.abs(images[0].astype(int)-x.astype(int)).max()) for x in images[1:])>2,name
        equivalence='mask' if kind=='document-mask' else kind
        if equivalence in baseline:assert all(np.array_equal(a,b) for a,b in zip(baseline[equivalence],images)),(name,'descriptor or document paths differ')
        else:baseline[equivalence]=images
        records.append(dict(name=name,kind=kind,frames=frames,validationEnabled=validation,
            imageSha256=[hashlib.sha256((out/f'frames/frame_{i:06d}.png').read_bytes()).hexdigest() for i in range(frames)],
            modelSha256=hashlib.sha256((assets/'surface.gltf').read_bytes()).hexdigest(),levelSha256=hashlib.sha256((assets/'scene.azurelevel').read_bytes()).hexdigest()))
        (root/'progress.json').write_text(json.dumps(dict(records=records,metrics=metrics),indent=2));print(name,flush=True)

    kinds=['clear-only','base','mask','document-mask','blend','red-alpha','highlight','node-opacity','soft-half','soft-zero','depth-occluded',
        'empty-background','diagonal','orthographic','frame-color','repeat','clamp','independent-samplers','scroll','clock-frozen','clock-reverse',
        'coverage-clamp','coverage-soft','coverage-negative','view-near','view-far','view-zero','view-soft','view-diagonal','view-orthographic','view-mask']
    if selected:
        assert set(selected)<=set(kinds),'Unknown layer case';kinds=[kind for kind in kinds if kind in selected]
        if 'clear-only' not in kinds:kinds.insert(0,'clear-only')
    for fixed in (False,True):
        for kind in kinds:capture(('fixed-' if fixed else 'indexed-')+kind,kind,fixed=fixed,frames=3 if kind in ('scroll','clock-frozen','clock-reverse') else 1)
    assert fingerprint()==frozen,'Inputs changed during GPU regression'
    summary=dict(schemaVersion=1,status='passed',captures=len(records),frames=sum(x['frames'] for x in records),
        linearTolerance=tolerance,maximumLinearError=max(metrics.values()),descriptorExact=True,clearBackgroundLinear=clear_background.tolist(),
        records=records,metrics=metrics,inputFingerprints=frozen,
        scope='Public actual draws, complete layers, clock control, depth, projection and resource lifetime',
        boundaries=['Native runtime VFX constants and actual logistics scene remain independently verified.','Host pause commands require separate lifecycle coverage.'])
    (root/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps({k:summary[k] for k in ['status','captures','frames','maximumLinearError']}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');parser.add_argument('--case',action='append');args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.expected_validation,args.case)
    else:
        with tempfile.TemporaryDirectory(prefix='unlit-layers-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(args.executable,Path(temporary)/'check',args.expected_validation,args.case)
