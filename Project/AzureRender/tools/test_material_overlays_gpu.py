"""Verify overlay conversion through real rigid annotation and emission draws."""
import argparse
import base64
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import component, surface
from test_unlit_texture_layers_gpu import layer, profile, png_uri, linear


def run(player, output, validation):
    output.mkdir(parents=True, exist_ok=False)
    root=Path(__file__).resolve().parents[1]
    inputs=[player, Path(__file__), root/'tools/append_material_overlays.py',
            root/'tools/convert_material_semantics.py', root/'tools/create_visibility_fixture.py',
            root/'tools/test_unlit_texture_layers_gpu.py', *sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprint=lambda:{str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    frozen=fingerprint();records=[];images={}
    for fixed in [False, True]:
        for additive in [False, True]:
            name=('fixed-' if fixed else 'indexed-')+('emission' if additive else 'annotation')
            folder=output/name;assets=folder/'assets';assets.mkdir(parents=True)
            source=assets/'source.gltf';surface(source, double_sided=True)
            model=json.loads(source.read_text())
            blob=base64.b64decode(model['buffers'][0]['uri'].split(',')[1])+struct.pack('<8f', *([.5]*8))
            model['buffers'][0]=dict(byteLength=len(blob), uri='data:application/octet-stream;base64,'+base64.b64encode(blob).decode())
            view=len(model['bufferViews']);model['bufferViews'].append(dict(buffer=0, byteOffset=len(blob)-32, byteLength=32))
            uv=len(model['accessors']);model['accessors'].append(dict(bufferView=view, componentType=5126, count=4, type='VEC2'))
            model['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0']=uv
            receiver=np.array([.07,.09,.11])
            model['materials']=[dict(name='Target', doubleSided=True,
                pbrMetallicRoughness=dict(baseColorFactor=[*receiver,1]),
                extras=dict(azureSlotId='target', azureRenderMaterial=dict(schemaVersion=2,
                    **{'class':'generic'}, features=['unlit-surface'], surface=dict(schemaVersion=1,lighting='unlit'))))]
            source.write_text(json.dumps(model))
            p=profile();p.update(depthCompare='less-or-equal', composite='additive' if additive else 'alpha',
                tint=[.2,.4,.1,.5],blendTint=[0,0,0,0],layers=[layer('base',0)],
                softIntersection=dict(enabled=False,distance=1,bias=0),highlightExpansion=dict(threshold=1,gain=0))
            config=dict(schemaVersion=1, images=[dict(uri=png_uri(np.full((1,1,4),255,dtype=np.uint8)))],
                samplers=[dict(wrapS=33071,wrapT=33071,minFilter=9729,magFilter=9729)],textures=[dict(source=0,sampler=0)],
                overlays=[dict(sourceSlot='target',uvSet=0,material=dict(name='Overlay',alphaMode='BLEND',doubleSided=True,
                    extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},
                        features=['unlit-surface','unlit-texture-layers'],surface=p))))])
            config_path=folder/'overlays.json';config_path.write_text(json.dumps(config));model_path=assets/'overlaid.gltf'
            subprocess.run([sys.executable,str(root/'tools/append_material_overlays.py'),'--input',str(source),
                '--config',str(config_path),'--output',str(model_path)],check=True,capture_output=True)
            level=dict(schemaVersion=1,id=name,sceneType='character',resources=[dict(id='surface',asset='assets:/overlaid.gltf')],
                nodes=[dict(id='surface',resourceId='surface'),dict(id='camera',components={
                    'azure.transform':component('azure.transform',dict(translation=[0,0,3])),
                    'azure.camera':dict(type='azure.camera',version=2,data=dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=20))})],
                lights=[],renderSettings=dict(schemaVersion=20,sceneLighting=True,skyIntensity=0,platform=False,background=False,
                    stylized=False,silhouetteOutline=False,innerOutline=False,bloomEnabled=False,antiAliasing=0,
                    exposure=0,toneMapping=False,contrast=1,saturation=1))
            (assets/'scene.azurelevel').write_text(json.dumps(level))
            project=folder/'project.azureproject';project.write_text(json.dumps(dict(schemaVersion=1,id=name,name=name,
                mounts=[dict(name='assets',path='assets')],startupScene='assets:/scene.azurelevel')))
            command=[str(player),'--project',str(project),'--width','128','--height','128','--capture-dir',str(folder/'frames'),
                '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene']
            if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            startup=None
            if os.name=='nt':
                startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
            result=subprocess.run(command,cwd=folder,startupinfo=startup,capture_output=True,timeout=120)
            log=result.stdout+result.stderr;(folder/'player.log').write_bytes(log);text=log.decode(errors='replace')
            assert result.returncode==0 and 'Allocator after unload: buffers=0 images=0' in text,name
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text),name
            if validation!='any':assert ('Validation layer: enabled' in text)==(validation=='enabled'),name
            image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))
            expected=np.array(p['tint'][:3])*.5+receiver*(1 if additive else .5)
            error=float(np.max(abs(linear(image[64,64])-expected)));assert error<=.008,(name,error)
            if fixed:assert np.array_equal(image,images[additive]),name
            else:images[additive]=image
            assert hashlib.sha256(source.read_bytes()).hexdigest()==json.loads(Path(str(model_path)+'.overlays.json').read_text())['sourceSha256']
            records.append(dict(name=name,maximumLinearError=error,command=command));print(name,'passed',flush=True)
    assert fingerprint()==frozen
    report=dict(status='passed',captures=len(records),frames=len(records),descriptorExact=True,
                cases=records,inputFingerprints=frozen)
    (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any')
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='material-overlays-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temp:
            run(args.executable.resolve(),Path(temp)/'check',args.expected_validation)
