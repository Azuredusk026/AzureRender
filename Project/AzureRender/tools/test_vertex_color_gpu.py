"""Verify optional vertex RGBA streams through public models and material documents."""
import argparse
import base64
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import surface,component,pad_material_profile_index
from convert_material_semantics import complete_material_document
from test_unlit_texture_layers_gpu import profile,layer,linear,png_uri


def run(player,output,validation='any',case='vertex'):
    output.mkdir(parents=True,exist_ok=False)
    inputs=[player.resolve(),Path(__file__).resolve(),Path(__file__).with_name('test_unlit_texture_layers_gpu.py'),
        Path(__file__).with_name('create_visibility_fixture.py'),Path(__file__).with_name('convert_material_semantics.py'),
        *sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprint=lambda:{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    frozen=fingerprint();records=[];images={};maximum=0
    kinds=('heatmap','red-alpha','mask','document','ignored','missing','skinned','opacity') if case=='vertex' else (
        'view-front','view-back','view-invert','view-mask','view-document','view-orthographic','view-soft','view-depth',
        'view-additive','view-additive-document')
    if case=='coverage-layer':kinds=('coverage-red','coverage-alpha','coverage-mask','coverage-low','coverage-high',
        'coverage-soft','coverage-document','coverage-linear','coverage-uv','coverage-clock','coverage-disabled')
    if case=='uv-distortion':kinds=('distortion-red','distortion-rg','distortion-bias','distortion-mask',
        'distortion-document','distortion-clock','distortion-disabled','distortion-srgb','distortion-linear')
    for fixed in (False,True):
        for kind in kinds:
            name=('fixed-' if fixed else 'indexed-')+kind;root=output/name;assets=root/'assets';assets.mkdir(parents=True)
            surface(assets/'surface.gltf',double_sided=True);model=json.loads((assets/'surface.gltf').read_text())
            raw=bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',')[1]))
            def append(data,ctype,shape,count):
                while len(raw)%4:raw.append(0)
                start=len(raw);raw.extend(data);view=len(model['bufferViews']);accessor=len(model['accessors'])
                model['bufferViews'].append(dict(buffer=0,byteOffset=start,byteLength=len(data)))
                model['accessors'].append(dict(bufferView=view,componentType=ctype,type=shape,count=count));return accessor
            positions=np.frombuffer(bytes(raw[:48]),dtype='<f4').reshape(4,3)
            normal=np.array([math.sqrt(.75),0,-.5 if kind=='view-back' else .5])
            if case=='view-coverage':
                model['meshes'][0]['primitives'][0]['attributes']['NORMAL']=append(np.tile(normal,(4,1)).astype('<f4').tobytes(),5126,'VEC3',4)
            if kind in ('coverage-uv','coverage-clock') or case=='uv-distortion':
                model['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0']=append(struct.pack('<8f',*([.25,.25]*4)),5126,'VEC2',4)
            colors=np.column_stack([positions[:,0]+.5,positions[:,1]+.5,np.full(4,.25),.5+.5*positions[:,0]]).astype('<f4')
            if kind!='missing':model['meshes'][0]['primitives'][0]['attributes']['COLOR_0']=append(colors.tobytes(),5126,'VEC4',4)
            if kind=='skinned':
                inverse=append(np.eye(4,dtype='<f4').tobytes(),5126,'MAT4',1)
                joints=append(bytes(32),5123,'VEC4',4);weights=append(struct.pack('<16f',*([1,0,0,0]*4)),5126,'VEC4',4)
                model['meshes'][0]['primitives'][0]['attributes'].update(JOINTS_0=joints,WEIGHTS_0=weights)
                model['skins']=[dict(joints=[1],inverseBindMatrices=inverse)]
                model['nodes'].append(dict(name='IdentityJoint'));model['nodes'][0]['skin']=0
                model['scenes'][0]['nodes'].append(1)
            model['buffers'][0]=dict(byteLength=len(raw),uri='data:application/octet-stream;base64,'+base64.b64encode(raw).decode())
            p=profile();p['vertexColor']=kind!='ignored';p['tint']=[.75,.5,1.25,3 if kind=='opacity' else 1]
            if case=='view-coverage':
                p['viewCoverage']=dict(bias=.2,exponent=2,invertWeight=1 if kind=='view-invert' else .3,weight=.75)
                p['softIntersection']['enabled']=kind=='view-soft'
                if kind=='view-depth':p['viewDepthFade']=dict(near=[2,4],far=[10,20])
                if kind.startswith('view-additive'):p['composite']='additive'
            base=np.array([128,64,192,255] if kind=='red-alpha' else [255,255,255,255],dtype=np.uint8)
            mask=np.array([128,192,64,192],dtype=np.uint8)
            if kind=='red-alpha':p['layers'][0]['sampleMode']='red-as-alpha'
            if kind in ('mask','view-mask'):p['layers'].append(layer('mask',1))
            if kind in ('coverage-mask','coverage-document'):p['layers'].append(layer('mask',1))
            model['images']=[dict(uri=png_uri(base.reshape(1,1,4))),dict(uri=png_uri(mask.reshape(1,1,4)))]
            model['textures']=[dict(source=0),dict(source=1)]
            coverage_pixel=np.array([128,64,224,192],dtype=np.uint8)
            if case=='coverage-layer':
                c=layer('mask',2);c.pop('role');c.update(sampleMode='rgba' if kind=='coverage-alpha' else 'red-as-alpha',threshold=.05,gain=2)
                if kind=='coverage-low':c['threshold']=1
                if kind=='coverage-high':c['threshold']=-1
                if kind!='coverage-disabled':p['coverageLayer']=c
                if kind in ('coverage-uv','coverage-clock'):
                    pattern=np.array([[[64,32,128,255],[224,32,128,255]],[[128,32,128,255],[192,32,128,255]]],dtype=np.uint8)
                    model['images'].append(dict(uri=png_uri(pattern)));coverage_pixel=pattern[0,1 if kind=='coverage-clock' else 0]
                    if kind=='coverage-clock':p['clock']=dict(rate=0,offsetSeconds=1);c['uv']['speed']=[.5,0]
                else:model['images'].append(dict(uri=png_uri(coverage_pixel.reshape(1,1,4))))
                model['textures'].append(dict(source=2,sampler=2))
                nearest=kind in ('coverage-uv','coverage-clock')
                model['samplers']=[dict(wrapS=10497,wrapT=10497,minFilter=9729,magFilter=9729),
                    dict(wrapS=33071,wrapT=33071,minFilter=9729,magFilter=9729),
                    dict(wrapS=33071,wrapT=33071,minFilter=9728 if nearest else 9729,magFilter=9728 if nearest else 9729)]
                model['textures'][0]['sampler']=0;model['textures'][1]['sampler']=1
                if kind=='coverage-linear':
                    path=assets/'coverage.png';Image.fromarray(coverage_pixel.reshape(1,1,4),'RGBA').save(path)
                    Path(str(path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id='55555555-5555-4555-8555-555555555555',settings=dict(texture=dict(colorSpace='linear')),dependencies=[])))
                    model['images'][2]=dict(uri='coverage.png')
            if case=='uv-distortion':
                pattern=np.array([[[255,64,128,192],[32,192,64,128]],[[64,128,255,64],[192,255,32,255]]],dtype=np.uint8)
                noise_pixel=np.array([128,224,64,255],dtype=np.uint8)
                noise_pattern=noise_pixel.reshape(1,1,4)
                if kind=='distortion-clock':
                    noise_pattern=np.array([[[0,32,128,255],[224,32,128,255]],[[64,32,128,255],[192,32,128,255]]],dtype=np.uint8)
                    noise_pixel=noise_pattern[0,1]
                model['images']=[dict(uri=png_uri(pattern)),dict(uri=png_uri(np.tile(mask,(2,2,1)))),dict(uri=png_uri(noise_pattern))]
                model['textures']=[dict(source=i,sampler=i) for i in range(3)]
                model['samplers']=[dict(wrapS=33071,wrapT=33071,minFilter=9728,magFilter=9728) for _ in range(3)]
                d=dict(texture=2,channels='rg' if kind=='distortion-rg' else 'red',uv=copy.deepcopy(p['layers'][0]['uv']),
                    scale=[.75 if kind in ('distortion-srgb','distortion-linear') else 2,.5 if kind=='distortion-rg' else 0],bias=[-.5,.5] if kind=='distortion-bias' else [0,0])
                if kind!='distortion-disabled':p['baseUvDistortion']=d
                if kind=='distortion-clock':p['clock']=dict(rate=0,offsetSeconds=1);d['uv']['speed']=[.5,0]
                if kind=='distortion-mask':
                    p['layers'].append(layer('mask',1));p['coverageLayer']=dict(texture=1,sampleMode='red-as-alpha',uv=copy.deepcopy(p['layers'][0]['uv']),threshold=0,gain=1)
                    mask_pattern=np.array([[mask,[255,255,255,255]],[[0,0,0,0],[0,0,0,0]]],dtype=np.uint8);model['images'][1]=dict(uri=png_uri(mask_pattern))
                if kind=='distortion-linear':
                    path=assets/'noise.png';Image.fromarray(noise_pattern,'RGBA').save(path)
                    Path(str(path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id='55555555-5555-4555-8555-555555555555',settings=dict(texture=dict(colorSpace='linear')),dependencies=[])));model['images'][2]=dict(uri=path.name)
                signal=(noise_pixel[:2]/255 if kind=='distortion-linear' else linear(noise_pixel[:2]));signal=signal if d['channels']=='rg' else np.repeat(signal[0],2)
                uv=np.array([.25,.25])+(signal*np.array(d['scale'])+d['bias'] if kind!='distortion-disabled' else 0)
                selected=np.clip(np.floor(uv*2).astype(int),0,1);base=pattern[selected[1],selected[0]]
            model['materials']=[dict(name='VertexSignal',alphaMode='BLEND',doubleSided=True,extras=dict(azureSlotId='surface',
                azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface','unlit-texture-layers'],surface=p)))]
            node=dict(id='surface',resourceId='surface',components={'azure.transform':component('azure.transform',dict(translation=[0,0,0]))})
            opacity=.4 if kind in ('opacity','view-soft','coverage-soft') else 1
            model['extensionsUsed']=['AZURE_node_opacity'];model['nodes'][0]['extensions']={'AZURE_node_opacity':dict(schemaVersion=1,opacity=opacity)}
            if kind in ('document','view-document','view-additive-document'):
                image_id='11111111-1111-4111-8111-111111111111';material_id='22222222-2222-4222-8222-222222222222'
                image=assets/'base.png';Image.fromarray(base.reshape(1,1,4),'RGBA').save(image)
                Path(str(image)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=image_id,settings={},dependencies=[])))
                model['images'][0]=dict(uri='base.png')
                document=complete_material_document(model['materials'][0],model,{'0':image_id})
                path=assets/'vertex.azurematerial';path.write_text(json.dumps(document))
                Path(str(path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,settings={},dependencies=[image_id])))
                node['materialSlots']={'surface':material_id}
                model['materials'][0]['extras']['azureRenderMaterial']['surface']=dict(p,vertexColor=False)
                if kind in ('view-document','view-additive-document'):
                    embedded=model['materials'][0]['extras']['azureRenderMaterial']['surface']
                    embedded['viewCoverage']=dict(p['viewCoverage'],weight=0);embedded['composite']='alpha'
            if kind=='coverage-document':
                ids=['11111111-1111-4111-8111-111111111111','22222222-2222-4222-8222-222222222222','33333333-3333-4333-8333-333333333333'];material_id='44444444-4444-4444-8444-444444444444'
                for i,value in enumerate([base,mask,coverage_pixel]):
                    path=assets/f'channel-{i}.png';Image.fromarray(value.reshape(1,1,4),'RGBA').save(path)
                    Path(str(path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=ids[i],settings={},dependencies=[])));model['images'][i]=dict(uri=path.name)
                document=complete_material_document(model['materials'][0],model,{str(i):ids[i] for i in range(3)})
                path=assets/'coverage.azurematerial';path.write_text(json.dumps(document));Path(str(path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,settings={},dependencies=ids)));node['materialSlots']={'surface':material_id}
                embedded=dict(p,vertexColor=False);embedded.pop('coverageLayer');model['materials'][0]['extras']['azureRenderMaterial']['surface']=embedded
            if kind=='distortion-document':
                ids=['11111111-1111-4111-8111-111111111111','22222222-2222-4222-8222-222222222222','33333333-3333-4333-8333-333333333333'];material_id='44444444-4444-4444-8444-444444444444'
                for i,value in enumerate([pattern,np.tile(mask,(2,2,1)),noise_pattern]):
                    path=assets/f'distortion-{i}.png';Image.fromarray(value,'RGBA').save(path)
                    Path(str(path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=ids[i],settings={},dependencies=[])));model['images'][i]=dict(uri=path.name)
                document=complete_material_document(model['materials'][0],model,{str(i):ids[i] for i in range(3)})
                path=assets/'distortion.azurematerial';path.write_text(json.dumps(document));Path(str(path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,settings={},dependencies=ids)));node['materialSlots']={'surface':material_id}
                embedded=dict(p);embedded.pop('baseUvDistortion');model['materials'][0]['extras']['azureRenderMaterial']['surface']=embedded
            pad_material_profile_index(model)
            (assets/'surface.gltf').write_text(json.dumps(model))
            surface(assets/'rear.gltf',double_sided=True);rear=json.loads((assets/'rear.gltf').read_text())
            rear['materials']=[dict(pbrMetallicRoughness=dict(baseColorFactor=[.07,.09,.11,1]),
                extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface'],surface=dict(schemaVersion=1,lighting='unlit'))))]
            (assets/'rear.gltf').write_text(json.dumps(rear))
            delta=.75 if kind in ('opacity','view-soft','coverage-soft') else 2
            camera=dict(id='camera',components={'azure.transform':component('azure.transform',dict(translation=[0,0,3])),
                'azure.camera':dict(type='azure.camera',version=2,data=dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=20))})
            if kind=='view-orthographic':camera['components']['azure.camera']['data'].update(orthographic=True,orthographicHeight=2)
            settings=dict(schemaVersion=20,sceneLighting=True,skyIntensity=0,platform=False,background=False,stylized=False,
                silhouetteOutline=False,innerOutline=False,bloomEnabled=False,antiAliasing=0,exposure=0,toneMapping=False,contrast=1,saturation=1)
            scene=dict(schemaVersion=1,id=name,sceneType='character',resources=[dict(id='surface',asset='assets:/surface.gltf'),dict(id='rear',asset='assets:/rear.gltf')],
                nodes=[node,camera,dict(id='rear',resourceId='rear',components={'azure.transform':component('azure.transform',dict(translation=[0,0,-delta],scale=[10,10,10]))})],lights=[],renderSettings=settings)
            (assets/'scene.azurelevel').write_text(json.dumps(scene));project=root/'project.azureproject'
            project.write_text(json.dumps(dict(schemaVersion=1,id='vertex-color',name='Public vertex color',mounts=[dict(name='assets',path='assets')],startupScene='assets:/scene.azurelevel')))
            command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128','--capture-dir',str((root/'frames').resolve()),
                '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--runtime-report',str((root/'runtime.json').resolve())]
            if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (root/'command.json').write_text(json.dumps(command,indent=2))
            with (root/'player.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(root/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log and not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            enabled='Validation layer: enabled' in log
            if validation!='any':assert enabled==(validation=='enabled'),name
            manifest=json.loads((root/'frames/capture_manifest.json').read_text());assert manifest['materialSurfaceProfileLayoutVersion']==17
            pixels=np.asarray(Image.open(root/'frames/frame_000000.png').convert('RGB'));errors=[]
            for x,y in [(64,64),(56,56),(72,56),(56,72),(72,72)]:
                world_x=(2*(x+.5)/128-1)*3*math.tan(math.radians(55/2))
                world_y=(1-2*(y+.5)/128)*3*math.tan(math.radians(55/2))
                if kind=='view-orthographic':world_x=(2*(x+.5)/128-1);world_y=(1-2*(y+.5)/128)
                vertex=np.array([world_x+.5,world_y+.5,.25,.5+.5*world_x]) if kind not in ('ignored','missing') else np.ones(4)
                rgb=linear(base[:3]);alpha=base[3]/255
                if kind=='red-alpha':alpha=rgb[0];rgb=np.ones(3)
                rgb=rgb*np.array(p['tint'][:3])*vertex[:3];alpha=alpha*p['tint'][3]*vertex[3]
                if kind in ('mask','view-mask','coverage-mask','coverage-document','distortion-mask'):rgb*=linear(mask[:3]);alpha*=mask[3]/255
                if kind=='distortion-mask':alpha*=linear(mask[:3])[0]
                if case=='coverage-layer' and kind!='coverage-disabled':
                    c=p['coverageLayer'];value=coverage_pixel[3]/255 if c['sampleMode']=='rgba' else coverage_pixel[0]/255 if kind=='coverage-linear' else linear(coverage_pixel[:3])[0]
                    alpha*=np.clip((value-c['threshold'])*c['gain'],0,1)
                soft=min(1,delta/1.5)
                if case=='view-coverage':
                    direction=np.array([-world_x,-world_y,3.]);direction/=np.linalg.norm(direction)
                    if kind=='view-orthographic':direction=np.array([0,0,1.])
                    v=p['viewCoverage'];power=np.clip(np.dot(normal,direction)+v['bias'],0,1)**v['exponent']
                    factor=1-v['weight']+v['weight']*((1-power)*(1-v['invertWeight'])+power*v['invertWeight'])
                    alpha=np.clip(alpha,0,1)*factor
                    if not p['softIntersection']['enabled']:soft=1
                    if kind=='view-depth':soft*=.5
                alpha=np.clip(alpha*soft,0,1)*opacity
                expected=np.clip(rgb*alpha+np.array([.07,.09,.11])*(1 if p.get('composite')=='additive' else 1-alpha),0,1)
                error=float(abs(linear(pixels[y,x])-expected).max());errors.append(error);assert error<=.008,(name,x,y,error,pixels[y,x].tolist(),expected.tolist())
            equivalence='heatmap' if kind in ('document','skinned') else 'ignored' if kind=='missing' else 'view-front' if kind=='view-document' else 'view-additive' if kind=='view-additive-document' else 'coverage-mask' if kind=='coverage-document' else kind
            if kind=='coverage-disabled':equivalence='coverage-high'
            if kind=='distortion-document':equivalence='distortion-red'
            if equivalence in images:assert np.array_equal(images[equivalence],pixels),(name,'descriptor, skinning or document mismatch')
            else:images[equivalence]=pixels
            maximum=max(maximum,max(errors));records.append(dict(name=name,kind=kind,maximumLinearError=max(errors),validationEnabled=enabled,
                imageSha256=hashlib.sha256((root/'frames/frame_000000.png').read_bytes()).hexdigest()))
            (output/'progress.json').write_text(json.dumps(records,indent=2));print(name,flush=True)
    assert fingerprint()==frozen,'Inputs changed during vertex color verification'
    report=dict(schemaVersion=1,status='passed',captures=len(records),maximumLinearError=maximum,linearTolerance=.008,records=records,inputFingerprints=frozen)
    (output/'summary.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ['status','captures','maximumLinearError']}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any')
    parser.add_argument('--case',choices=['vertex','view-coverage','coverage-layer','uv-distortion'],default='vertex');args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.expected_validation,args.case)
    else:
        with tempfile.TemporaryDirectory(prefix='vertex-color-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(args.executable,Path(temporary)/'check',args.expected_validation,args.case)
