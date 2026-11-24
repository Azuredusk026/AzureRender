"""Verify surface overlay radiance in the real reflection snapshot."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image
from test_screen_space_reflections_gpu import prepare, measure_reflection, polygon_mask, SOURCE_RGB
from test_unlit_texture_layers_gpu import profile, layer, png_uri, linear


def run(player, output, validation, anti_aliasing=0):
    output.mkdir(parents=True,exist_ok=False)
    inputs=[player,Path(__file__),Path(__file__).with_name('test_screen_space_reflections_gpu.py'),
            Path(__file__).with_name('test_unlit_texture_layers_gpu.py'),*sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprint=lambda:{str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    frozen=fingerprint();images={};records=[]
    for fixed in [False,True]:
        backend='fixed' if fixed else 'indexed'
        for case in ['reflection-off','reflection-black','reflection-transparent','reflection-overlay','beauty-transparent','beauty-overlay','beauty-overlay-inspection','beauty-overlay-transparent-sheet']:
            name=backend+'-'+case;folder=output/name
            project,expected=prepare(folder,dict(name=name,enabled=case!='reflection-off',antiAliasing=anti_aliasing))
            source=project.parent/'assets/target.gltf';model=json.loads(source.read_text())
            raw=base64.b64decode(model['buffers'][0]['uri'].split(',')[1])
            raw+=b'\0'*(-len(raw)%4);offset=len(raw);raw+=struct.pack('<8f',*([.5]*8))
            view=len(model['bufferViews']);model['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=32))
            uv=len(model['accessors']);model['accessors'].append(dict(bufferView=view,componentType=5126,count=4,type='VEC2'))
            model['buffers'][0]=dict(byteLength=len(raw),uri='data:application/octet-stream;base64,'+base64.b64encode(raw).decode())
            primitive=model['meshes'][0]['primitives'][0];primitive['attributes']['TEXCOORD_0']=uv
            receiver=dict(name='Receiver',doubleSided=True,pbrMetallicRoughness=dict(baseColorFactor=[0,0,0,1]),
                extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface'],
                    surface=dict(schemaVersion=1,lighting='unlit'))))
            p=profile();p.update(composite='additive',depthCompare='less-or-equal',submission='opaque-overlay' if 'overlay' in case or case in ['reflection-off','reflection-black'] else 'transparent',
                tint=[*SOURCE_RGB,1],blendTint=[0,0,0,0],layers=[layer('base',0)],
                softIntersection=dict(enabled=False,distance=1,bias=0),highlightExpansion=dict(threshold=1,gain=0))
            if case=='reflection-black':p['tint']=[0,0,0,1]
            overlay=dict(name='SurfaceRadiance',alphaMode='BLEND',doubleSided=True,
                extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},
                    features=['unlit-surface','unlit-texture-layers'],surface=p)))
            model['materials']=[receiver,overlay];primitive['material']=0
            extra=copy.deepcopy(primitive);extra['material']=1;model['meshes'][0]['primitives'].append(extra)
            model['images']=[dict(uri=png_uri(np.full((1,1,4),255,dtype=np.uint8)))];model['textures']=[dict(source=0)]
            if case=='beauty-overlay-inspection':
                p['vertexColor']=True
                colors=[.5,.25,1,1]
                offset=len(raw);raw+=struct.pack('<16f',*(colors*4))
                view=len(model['bufferViews']);model['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=64))
                accessor=len(model['accessors']);model['accessors'].append(dict(bufferView=view,componentType=5126,count=4,type='VEC4'))
                for primitive in model['meshes'][0]['primitives']:primitive['attributes']['COLOR_0']=accessor
                model['buffers'][0]=dict(byteLength=len(raw),uri='data:application/octet-stream;base64,'+base64.b64encode(raw).decode())
            if case=='beauty-overlay-transparent-sheet':
                sheet=dict(name='InspectionSheet',alphaMode='BLEND',doubleSided=True,
                    pbrMetallicRoughness=dict(baseColorFactor=[0,0,.4,.5]),
                    extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface'],
                        surface=dict(schemaVersion=1,lighting='unlit'))))
                model['materials'].append(sheet)
                extra=copy.deepcopy(model['meshes'][0]['primitives'][0]);extra['material']=2
                model['meshes'].append(dict(primitives=[extra]));model['nodes'].append(dict(mesh=1,translation=[0,0,.01]))
                model['scenes'][0]['nodes'].append(len(model['nodes'])-1)
            source.write_text(json.dumps(model))
            isolation='beauty' if case.startswith('beauty') else 'reflection'
            command=[str(player),'--project',str(project.resolve()),'--width','320','--height','240',
                '--capture-dir',str((folder/'frames').resolve()),'--capture-frames','1','--fixed-frame-step',
                '--qa-camera','scene','--qa-light','scene','--qa-isolation',isolation]
            if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            startup=None
            if hasattr(subprocess,'STARTUPINFO'):
                startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
            result=subprocess.run(command,capture_output=True,timeout=180,startupinfo=startup)
            log=result.stdout+result.stderr;(folder/'player.log').write_bytes(log);text=log.decode(errors='replace')
            assert result.returncode==0 and 'Allocator after unload: buffers=0 images=0' in text,(name,text[-1000:])
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text),name
            if validation!='any':assert ('Validation layer: enabled' in text)==(validation=='enabled'),name
            assert f'Character texture access: {backend}' in text,name
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text())
            assert manifest['antiAliasing']==anti_aliasing and manifest['qaIsolation']==isolation,name
            image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))
            images[backend,case]=image
            if fixed:assert np.array_equal(image,images['indexed',case]),name
            records.append(dict(name=name,command=command));print(name,'captured',flush=True)
        report=measure_reflection(images[backend,'reflection-overlay'],images[backend,'reflection-off'],expected,backend+'-surface-radiance')
        mask=polygon_mask(expected['reflectionPolygon'],.7)
        transparent_delta=abs(linear(images[backend,'reflection-transparent'])-linear(images[backend,'reflection-black']))
        assert float(transparent_delta[mask].max())<=.008,'Transparent effects retain snapshot exclusion'
        source_mask=polygon_mask(expected['sourcePolygon'],.5)
        for case in ['beauty-transparent','beauty-overlay','beauty-overlay-inspection','beauty-overlay-transparent-sheet']:
            expected_color=np.asarray(SOURCE_RGB)
            if case=='beauty-overlay-inspection':expected_color=expected_color*np.array([.5,.25,1])
            if case=='beauty-overlay-transparent-sheet':expected_color=expected_color*.5+np.array([0,0,.2])
            error=float(abs(linear(images[backend,case])[source_mask]-expected_color).max())
            assert error<=.008,(backend,case,error)
        records.append(report)
    assert fingerprint()==frozen
    (output/'report.json').write_text(json.dumps(dict(status='passed',frames=16,antiAliasing=anti_aliasing,descriptorExact=True,
        records=records,inputFingerprints=frozen),indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any')
    parser.add_argument('--anti-aliasing',type=int,choices=[0,4],default=0)
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.expected_validation,args.anti_aliasing)
    else:
        with tempfile.TemporaryDirectory(prefix='surface-overlay-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temp:
            run(args.executable.resolve(),Path(temp)/'check',args.expected_validation,args.anti_aliasing)
