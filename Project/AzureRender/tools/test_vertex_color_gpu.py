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
from create_visibility_fixture import surface,component
from convert_material_semantics import complete_material_document
from test_unlit_texture_layers_gpu import profile,layer,linear,png_uri


def run(player,output,validation='any'):
    output.mkdir(parents=True,exist_ok=False)
    inputs=[player.resolve(),Path(__file__).resolve(),Path(__file__).with_name('test_unlit_texture_layers_gpu.py'),
        Path(__file__).with_name('create_visibility_fixture.py'),Path(__file__).with_name('convert_material_semantics.py'),
        *sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprint=lambda:{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    frozen=fingerprint();records=[];images={};maximum=0
    for fixed in (False,True):
        for kind in ('heatmap','red-alpha','mask','document','ignored','missing','skinned','opacity'):
            name=('fixed-' if fixed else 'indexed-')+kind;root=output/name;assets=root/'assets';assets.mkdir(parents=True)
            surface(assets/'surface.gltf',double_sided=True);model=json.loads((assets/'surface.gltf').read_text())
            raw=bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',')[1]))
            def append(data,ctype,shape,count):
                while len(raw)%4:raw.append(0)
                start=len(raw);raw.extend(data);view=len(model['bufferViews']);accessor=len(model['accessors'])
                model['bufferViews'].append(dict(buffer=0,byteOffset=start,byteLength=len(data)))
                model['accessors'].append(dict(bufferView=view,componentType=ctype,type=shape,count=count));return accessor
            positions=np.frombuffer(bytes(raw[:48]),dtype='<f4').reshape(4,3)
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
            base=np.array([128,64,192,255] if kind=='red-alpha' else [255,255,255,255],dtype=np.uint8)
            mask=np.array([128,192,64,192],dtype=np.uint8)
            if kind=='red-alpha':p['layers'][0]['sampleMode']='red-as-alpha'
            if kind=='mask':p['layers'].append(layer('mask',1))
            model['images']=[dict(uri=png_uri(base.reshape(1,1,4))),dict(uri=png_uri(mask.reshape(1,1,4)))]
            model['textures']=[dict(source=0),dict(source=1)]
            model['materials']=[dict(name='VertexSignal',alphaMode='BLEND',doubleSided=True,extras=dict(azureSlotId='surface',
                azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface','unlit-texture-layers'],surface=p)))]
            node=dict(id='surface',resourceId='surface',components={'azure.transform':component('azure.transform',dict(translation=[0,0,0]))})
            opacity=.4 if kind=='opacity' else 1
            model['extensionsUsed']=['AZURE_node_opacity'];model['nodes'][0]['extensions']={'AZURE_node_opacity':dict(schemaVersion=1,opacity=opacity)}
            if kind=='document':
                image_id='11111111-1111-4111-8111-111111111111';material_id='22222222-2222-4222-8222-222222222222'
                image=assets/'base.png';Image.fromarray(base.reshape(1,1,4),'RGBA').save(image)
                Path(str(image)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=image_id,settings={},dependencies=[])))
                model['images'][0]=dict(uri='base.png')
                document=complete_material_document(model['materials'][0],model,{'0':image_id})
                path=assets/'vertex.azurematerial';path.write_text(json.dumps(document))
                Path(str(path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,settings={},dependencies=[image_id])))
                node['materialSlots']={'surface':material_id}
                model['materials'][0]['extras']['azureRenderMaterial']['surface']=dict(p,vertexColor=False)
            (assets/'surface.gltf').write_text(json.dumps(model))
            surface(assets/'rear.gltf',double_sided=True);rear=json.loads((assets/'rear.gltf').read_text())
            rear['materials']=[dict(pbrMetallicRoughness=dict(baseColorFactor=[.07,.09,.11,1]),
                extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface'],surface=dict(schemaVersion=1,lighting='unlit'))))]
            (assets/'rear.gltf').write_text(json.dumps(rear))
            delta=.75 if kind=='opacity' else 2
            camera=dict(id='camera',components={'azure.transform':component('azure.transform',dict(translation=[0,0,3])),
                'azure.camera':dict(type='azure.camera',version=2,data=dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=20))})
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
            manifest=json.loads((root/'frames/capture_manifest.json').read_text());assert manifest['materialSurfaceProfileLayoutVersion']==13
            pixels=np.asarray(Image.open(root/'frames/frame_000000.png').convert('RGB'));errors=[]
            for x,y in [(64,64),(56,56),(72,56),(56,72),(72,72)]:
                world_x=(2*(x+.5)/128-1)*3*math.tan(math.radians(55/2))
                world_y=(1-2*(y+.5)/128)*3*math.tan(math.radians(55/2))
                vertex=np.array([world_x+.5,world_y+.5,.25,.5+.5*world_x]) if kind not in ('ignored','missing') else np.ones(4)
                rgb=linear(base[:3]);alpha=base[3]/255
                if kind=='red-alpha':alpha=rgb[0];rgb=np.ones(3)
                rgb=rgb*np.array(p['tint'][:3])*vertex[:3];alpha=alpha*p['tint'][3]*vertex[3]
                if kind=='mask':rgb*=linear(mask[:3]);alpha*=mask[3]/255
                alpha=np.clip(alpha*min(1,delta/1.5),0,1)*opacity
                expected=np.clip(rgb*alpha+np.array([.07,.09,.11])*(1-alpha),0,1)
                error=float(abs(linear(pixels[y,x])-expected).max());errors.append(error);assert error<=.008,(name,x,y,error,pixels[y,x].tolist(),expected.tolist())
            equivalence='heatmap' if kind in ('document','skinned') else 'ignored' if kind=='missing' else kind
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
    parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='vertex-color-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(args.executable,Path(temporary)/'check',args.expected_validation)
