"""Verify captured-view fog through public scene, material and Player interfaces."""
import argparse
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
from create_visibility_fixture import component, surface, pad_material_profile_index
from convert_material_semantics import complete_material_document
from test_unlit_texture_layers_gpu import profile, png_uri, linear


CASES=('alpha','additive','none','disabled','absent','camera-mismatch','aspect-mismatch',
       'near','orthographic','gradient','resize','document','msaa')


def run(player, output, validation, selected):
    output.mkdir(parents=True,exist_ok=False)
    dependencies=[player,Path(__file__),Path(__file__).with_name('create_visibility_fixture.py'),
                  Path(__file__).with_name('convert_material_semantics.py'),
                  Path(__file__).with_name('test_unlit_texture_layers_gpu.py'),
                  *sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprint=lambda:{str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in dependencies}
    frozen=fingerprint();records=[];images={};maximum=0
    for fixed in (False,True):
        for kind in selected or CASES:
            name=('fixed-' if fixed else 'indexed-')+kind
            root=output/name;assets=root/'assets';assets.mkdir(parents=True)
            surface(assets/'surface.gltf',double_sided=True)
            model=json.loads((assets/'surface.gltf').read_text())
            p=profile();p.update(fogMode='none' if kind=='none' else 'scene',tint=[.45,.25,.1,.5],
                blendTint=[0,0,0,0],softIntersection=dict(enabled=False,distance=1,bias=0),
                highlightExpansion=dict(threshold=1,gain=0))
            if kind=='additive':p['composite']='additive'
            model['images']=[dict(uri=png_uri(np.full((1,1,4),255,dtype=np.uint8)))]
            model['textures']=[dict(source=0)]
            model['materials']=[dict(name='FogSurface',alphaMode='BLEND',doubleSided=True,
                extras=dict(azureSlotId='surface',azureRenderMaterial=dict(schemaVersion=2,
                **{'class':'generic'},features=['unlit-surface','unlit-texture-layers'],surface=p)))]
            front=dict(id='surface',resourceId='surface',components={'azure.transform':component('azure.transform',dict(translation=[0,0,0]))})
            if kind=='document':
                image_id='11111111-1111-4111-8111-111111111111';material_id='22222222-2222-4222-8222-222222222222'
                Image.new('RGBA',(1,1),(255,255,255,255)).save(assets/'white.png')
                (assets/'white.png.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=image_id,settings={},dependencies=[])))
                model['images'][0]=dict(uri='white.png')
                document=complete_material_document(model['materials'][0],model,{'0':image_id})
                (assets/'fog.azurematerial').write_text(json.dumps(document))
                (assets/'fog.azurematerial.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,settings={},dependencies=[image_id])))
                front['materialSlots']={'surface':material_id}
                model['materials'][0]['extras']['azureRenderMaterial']['surface']=dict(p,fogMode='none')
            pad_material_profile_index(model)
            (assets/'surface.gltf').write_text(json.dumps(model))
            surface(assets/'rear.gltf',double_sided=True)
            rear=json.loads((assets/'rear.gltf').read_text())
            rear['materials']=[dict(pbrMetallicRoughness=dict(baseColorFactor=[.07,.09,.11,1]),
                extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface'],
                surface=dict(schemaVersion=1,lighting='unlit'))))]
            (assets/'rear.gltf').write_text(json.dumps(rear))
            size=256 if kind=='resize' else 128
            camera=dict(id='camera',components={'azure.transform':component('azure.transform',dict(translation=[0,0,3])),
                'azure.camera':dict(type='azure.camera',version=2,data=dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=20))})
            if kind=='orthographic':camera['components']['azure.camera']['data'].update(orthographic=True,orthographicHeight=2)
            view=dict(projection='orthographic' if kind=='orthographic' else 'perspective',position=[0,0,3],target=[0,0,2],up=[0,1,0],
                fieldOfViewRadians=math.radians(55),orthographicHeight=2 if kind=='orthographic' else 5,
                nearPlane=.1,farPlane=20,projectionOffset=[0,0])
            if kind=='camera-mismatch':view['position'][0]=.25
            projection=dict(schemaVersion=1,extent=[128,64 if kind=='aspect-mismatch' else 128],pixelOrigin='upper-left',
                framePhase=0,pixelCenterOffset=.5,jitterRadius=0,depthEncoding=[0,1,1],minimumViewDepth=4 if kind=='near' else 0,
                nearFadeWidth=2 if kind=='near' else .000001)
            gradient=kind in ('gradient','resize')
            dims=(2,2,2) if gradient else (1,1,1)
            payload=bytearray()
            for z in range(dims[2]):
                for y in range(dims[1]):
                    for x in range(dims[0]):payload.extend(struct.pack('<4e',*(x*.125,y*.0625,.03125,.5) if gradient else (.03125,.0625,.125,.5)))
            (assets/'view.azfog').write_bytes(b'AZFOG001'+struct.pack('<3I',*dims)+payload)
            settings=dict(schemaVersion=20,sceneLighting=True,skyIntensity=0,platform=False,background=False,stylized=False,
                silhouetteOutline=False,innerOutline=False,bloomEnabled=False,antiAliasing=3 if kind=='msaa' else 0,
                exposure=0,toneMapping=False,contrast=1,saturation=1)
            scene=dict(schemaVersion=1,id=name,sceneType='character',resources=[dict(id='surface',asset='assets:/surface.gltf'),dict(id='rear',asset='assets:/rear.gltf')],
                nodes=[front,camera,dict(id='rear',resourceId='rear',components={'azure.transform':component('azure.transform',dict(translation=[0,0,-2],scale=[10,10,10]))})],
                lights=[],renderSettings=settings)
            if kind!='absent':scene['viewFog']=dict(schemaVersion=1,asset='assets:/view.azfog',enabled=kind!='disabled',projection=projection,captureView=view)
            (assets/'scene.azurelevel').write_text(json.dumps(scene))
            project=root/'project.azureproject';project.write_text(json.dumps(dict(schemaVersion=1,id='view-fog',name='View fog GPU fixture',
                mounts=[dict(name='assets',path='assets')],startupScene='assets:/scene.azurelevel')))
            command=[str(player),'--project',str(project.resolve()),'--width',str(size),'--height',str(size),'--capture-dir',str((root/'frames').resolve()),
                '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene']
            if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (root/'command.json').write_text(json.dumps(command,indent=2))
            with (root/'player.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(root/'player.log').read_text(errors='replace')
            assert 'Allocator after unload: buffers=0 images=0' in log and not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            if validation!='any':assert ('Validation layer: enabled' in log)==(validation=='enabled'),name
            manifest=json.loads((root/'frames/capture_manifest.json').read_text())
            assert manifest['materialSurfaceProfileLayoutVersion']==17 and manifest['cameraUniformBytes']==688,name
            expected_state=kind if kind in ('camera-mismatch','aspect-mismatch') else 'disabled' if kind in ('absent','disabled') else 'matched'
            assert manifest['viewFogMatch']==expected_state,(name,manifest['viewFogMatch'])
            pixels=np.asarray(Image.open(root/'frames/frame_000000.png').convert('RGB'));errors=[]
            for x,y,covered in [(size//2,size//2,True),(size//2+4,size//2-4,True),(size//10,size//10,False)]:
                scatter=np.array([.03125,.0625,.125]);trans=.5
                if gradient:
                    uv=(np.floor((np.array([x,y])+.5)*128/size)+.5)/128
                    xy=np.clip(uv*2-.5,0,1);scatter=np.array([xy[0]*.125,xy[1]*.0625,.03125]).astype(np.float16).astype(float)
                active=kind not in ('disabled','absent','camera-mismatch','aspect-mismatch')
                def fog(color,depth,additive=False):
                    weight=np.clip((depth-projection['minimumViewDepth'])/projection['nearFadeWidth'],0,1) if active else 0
                    return color*(1+(trans-1)*weight)+(0 if additive else scatter*weight)
                expected=fog(np.array([.07,.09,.11]),5)
                if covered:
                    front_color=np.array([.45,.25,.1]) if kind=='none' else fog(np.array([.45,.25,.1]),3,kind=='additive')
                    expected=expected+front_color*.5 if kind=='additive' else expected*.5+front_color*.5
                actual=linear(pixels[y,x]);error=float(np.max(abs(actual-expected)));errors.append(error)
                assert error<=.008,(name,x,y,actual.tolist(),expected.tolist(),error)
            if fixed:assert np.array_equal(pixels,images[kind]),name
            else:images[kind]=pixels
            maximum=max(maximum,*errors);records.append(dict(case=name,maximumLinearError=max(errors),viewMatch=expected_state))
            print(name,'passed',flush=True)
    assert fingerprint()==frozen,'GPU test inputs changed during execution'
    report=dict(status='passed',cases=records,maximumLinearError=maximum,budget=.008,descriptorExact=True,inputFingerprints=frozen)
    (output/'report.json').write_text(json.dumps(report,indent=2));print('View fog checks:',len(records),'maximum linear error:',maximum)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--expected-validation',choices=('any','enabled','disabled'),default='any')
    parser.add_argument('--case',choices=CASES,action='append')
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.expected_validation,args.case)
    else:
        with tempfile.TemporaryDirectory(prefix='view-fog-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(args.executable.resolve(),Path(temporary)/'check',args.expected_validation,args.case)
