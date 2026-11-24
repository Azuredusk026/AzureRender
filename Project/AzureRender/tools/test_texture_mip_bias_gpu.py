"""Verify authored mip bias, sampler identity and complete material inputs."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import uuid

import numpy as np
from PIL import Image
from native_render.light_lab import generate
from convert_material_semantics import complete_material_document
from test_unlit_texture_layers_gpu import profile as layer_profile,layer


def authored_mips():
    colors=((255,0,0,255),(0,255,0,255),(0,0,255,255),(255,255,0,255),
            (255,0,255,255),(0,255,255,255),(128,128,128,255),(255,255,255,255))
    words=[124,0x2100f,128,128,512,0,8]+[0]*11
    words += [32,0x41,0,32,255,65280,16711680,4278190080,0x401008,0,0,0,0]
    return b'DDS '+struct.pack('<31I',*words)+b''.join(bytes(color)*(128>>i)**2 for i,color in enumerate(colors))

def coverage_mips():
    raw=authored_mips()
    return raw[:128]+b''.join(bytes([0 if i in (1,3) else 255,255,255,255])*(128>>i)**2 for i in range(8))


def run(player,output,expected_validation='any',selected=None):
    output.mkdir(parents=True,exist_ok=False)
    names=('neutral','biased','fixed','documents','device-clamped','mip-filter',
           'layer-neutral','layer-biased','layer-fixed','layer-documents','layer-mask-biased',
           'coverage-neutral','coverage-biased','coverage-fixed','coverage-documents',
           'distortion-neutral','distortion-biased','distortion-fixed','distortion-documents')
    if selected:
        assert set(selected)<=set(names);names=tuple(name for name in names if name in selected)
    report={'schemaVersion':1,'status':'running','captures':[],
            'scope':'Implicit LOD 2 from 128 texels over 32 pixels; authored mip colors and public image metadata',
            'playerSha256':hashlib.sha256(player.read_bytes()).hexdigest()}
    images={};statistics={}
    try:
        for name in names:
            print(name,flush=True)
            covered=name.startswith('coverage-');disturbed=name.startswith('distortion-');layered=name.startswith('layer-') or covered or disturbed
            variant=name.removeprefix('distortion-') if disturbed else name.removeprefix('coverage-') if covered else name.removeprefix('layer-')
            root=output/name;generate(root,'dark')
            model=json.loads((root/'plane.gltf').read_text(encoding='utf-8'))
            model['materials'][0]['name']='MipPanel'
            model['materials'][0]['pbrMetallicRoughness']['baseColorTexture']={'index':0}
            model['textures']=[{'source':0}]
            nodes=[];resources=[]
            for index,side in enumerate(('left','right')):
                identity=str(uuid.uuid4());bias=0 if variant in ('neutral','mip-filter') else ((-16 if index==0 else 16) if variant=='device-clamped' else (-1 if index==0 else 1))
                path=root/(side+'.dds');path.write_bytes(coverage_mips() if covered or disturbed else authored_mips())
                metadata={'schemaVersion':1,'id':identity,'dependencies':[],
                          'settings':{'texture':{'filter':'linear','mipFilter':'linear' if name=='mip-filter' and index==1 else 'nearest','anisotropy':1,'mipBias':bias}}}
                Path(str(path)+'.azmeta').write_text(json.dumps(metadata),encoding='utf-8')
                asset=copy.deepcopy(model);asset['images']=[{'uri':side+'.dds'}]
                if layered:
                    surface=layer_profile();surface['softIntersection']['distance']=.001
                    asset['samplers']=[dict(minFilter=9985,magFilter=9729,wrapS=10497,wrapT=10497)]
                    asset['textures'][0]['sampler']=0
                    asset['materials'][0].pop('pbrMetallicRoughness',None)
                    asset['materials'][0].update(alphaMode='BLEND',doubleSided=True,extras=dict(azureRenderMaterial=
                        dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface','unlit-texture-layers'],surface=surface)))
                    if variant=='mask-biased' or covered:
                        white=root/'white.dds'
                        if not white.exists():
                            raw=authored_mips();white.write_bytes(raw[:128]+bytes([255,255,255,255])*(sum((128>>i)**2 for i in range(8))))
                        asset['images']=[{'uri':'white.dds'},{'uri':side+'.dds'}]
                        asset['textures']=[dict(source=0,sampler=0),dict(source=1,sampler=0)]
                        if covered:
                            coverage=layer('mask',1);coverage.pop('role');coverage.update(sampleMode='red-as-alpha',threshold=0,gain=1);surface['coverageLayer']=coverage
                            white_identity=str(uuid.uuid5(uuid.NAMESPACE_URL,str(white.resolve())))
                            Path(str(white)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=white_identity,settings={},dependencies=[])))
                        else:surface['layers'].append(layer('mask',1))
                if disturbed:
                    base_path=root/'pattern.png'
                    if not base_path.exists():Image.fromarray(np.array([[[255,0,0,255],[0,255,0,255]]],dtype=np.uint8),'RGBA').save(base_path)
                    white_identity=str(uuid.uuid5(uuid.NAMESPACE_URL,str(base_path.resolve())))
                    Path(str(base_path)+'.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=white_identity,settings={},dependencies=[])))
                    asset['images']=[dict(uri=base_path.name),dict(uri=side+'.dds')];asset['textures']=[dict(source=0,sampler=1),dict(source=1,sampler=0)]
                    asset['samplers'].append(dict(minFilter=9728,magFilter=9728,wrapS=33071,wrapT=33071))
                    surface['layers'][0]['uv']['scaleOffset']=[0,0,.25,.5]
                    noise=layer('base',1);surface['baseUvDistortion']=dict(texture=1,channels='red',uv=noise['uv'],scale=[.5,0],bias=[0,0])
                (root/(side+'.gltf')).write_text(json.dumps(asset),encoding='utf-8')
                resources.append({'id':side,'asset':'assets:/'+side+'.gltf'})
                node={'id':side,'resourceId':side,'components':{'azure.transform':{
                    'type':'azure.transform','version':1,'data':{'translation':[-9 if index==0 else 9,0,0]}}}}
                if variant=='documents':
                    document=complete_material_document(asset['materials'][0],asset,{'0':white_identity,'1':identity} if covered or disturbed else {'0':identity})
                    material=root/(side+'.azurematerial');material.write_text(json.dumps(document),encoding='utf-8')
                    material_id=str(uuid.uuid4())
                    Path(str(material)+'.azmeta').write_text(json.dumps({'schemaVersion':1,'id':material_id,'settings':{},'dependencies':[]}),encoding='utf-8')
                    node['materialSlots']={'name:MipPanel':material_id}
                nodes.append(node)
            path=root/'level.azurelevel';level=json.loads(path.read_text(encoding='utf-8'))
            camera=next(n for n in level['nodes'] if n['id']=='camera')
            camera['components']['azure.camera']['data']['orthographicHeight']=32*2**.25 if name=='mip-filter' else 32
            level.update(resources=resources,nodes=nodes+[camera],lights=[])
            if layered:level['renderSettings'].update(toneMapping=False,exposure=0,bloomEnabled=False,antiAliasing=0)
            path.write_text(json.dumps(level),encoding='utf-8')
            command=[str(player.resolve()),'--project',str((root/'lab.azureproject').resolve()),
                     '--width','128','--height','128','--capture-dir',str(root/'frames'),
                     '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene',
                     '--qa-isolation','beauty' if layered else 'albedo','--runtime-report',str(root/'runtime.json')]
            if variant=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (root/'command.json').write_text(json.dumps(command,indent=2),encoding='utf-8')
            with (root/'player.log').open('w',encoding='utf-8') as log:
                subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(root/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log,name
            validation='Validation layer: enabled' in log
            if expected_validation!='any':assert validation==(expected_validation=='enabled'),name
            manifest=json.loads((root/'frames/capture_manifest.json').read_text(encoding='utf-8'))
            capabilities=json.loads((root/'frames/gpu_capabilities.json').read_text(encoding='utf-8'))
            maximum_bias=capabilities['resource_access']['limits']['max_sampler_lod_bias']
            assert maximum_bias>=1,name
            runtime=json.loads((root/'runtime.json').read_text(encoding='utf-8'))
            assert not runtime['scriptErrors'] and not runtime['presentationErrors'],name
            path=root/'frames/frame_000000.png';pixels=np.asarray(Image.open(path).convert('RGB'))
            expected=np.array([(0,0,255),(0,0,255)] if variant=='neutral' else [(0,255,0),(255,255,0)],dtype=int)
            if covered:expected=np.full((2,3),255) if variant=='neutral' else np.tile(pixels[0,0],(2,1))
            if disturbed:expected=np.tile([0,255,0] if variant=='neutral' else [255,0,0],(2,1))
            if name=='device-clamped':
                palette=[(255,0,0),(0,255,0),(0,0,255),(255,255,0),(255,0,255),(0,255,255),(128,128,128),(255,255,255)]
                # Independent implicit-LOD oracle: 128 texels / 32 pixels,
                # then device-bounded bias and nearest authored level.
                expected=np.array([palette[min(7,max(0,int(np.floor(2+np.clip(b,-maximum_bias,maximum_bias)+.5))))] for b in (-16,16)])
            if name=='mip-filter':expected=np.array([(0,0,255),(137,137,225)])
            measured=np.array([np.median(pixels[61:67,x-3:x+3],axis=(0,1)) for x in ((34,94) if name=='mip-filter' else (28,100))])
            error=float(np.abs(expected-measured).max()/255)
            report['captures'].append({'name':name,'maximumByteError':float(np.abs(expected-measured).max()),
                'expected':expected.tolist(),'measured':measured.tolist(),'validationEnabled':validation,
                'imageSha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                'deviceMaximumMipBias':maximum_bias,
                'textureCache':manifest['materialTextureCache']})
            images[name]=pixels;statistics[name]=manifest['materialTextureCache']
            assert error<=1/255,(name,error,measured.tolist())
        for left,right in [('biased','fixed'),('biased','documents')]:
            if left in images and right in images:assert np.array_equal(images[left],images[right]),(left,right)
        for right in ('layer-fixed','layer-documents','layer-mask-biased'):
            if 'layer-biased' in images and right in images:assert np.array_equal(images['layer-biased'],images[right]),right
        for right in ('coverage-fixed','coverage-documents'):
            if 'coverage-biased' in images and right in images:assert np.array_equal(images['coverage-biased'],images[right]),right
        for right in ('distortion-fixed','distortion-documents'):
            if 'distortion-biased' in images and right in images:assert np.array_equal(images['distortion-biased'],images[right]),right
        if 'biased' in statistics and 'neutral' in statistics:
            assert statistics['biased']['uniqueTextures']==statistics['neutral']['uniqueTextures']+1
        report['status']='passed'
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:
        (output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--expected-validation',choices=('enabled','disabled','any'),default='any')
    parser.add_argument('--case',action='append')
    args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.expected_validation,args.case)
    else:
        with tempfile.TemporaryDirectory(prefix='sampler-mip-bias-',dir=Path(__file__).resolve().parents[1]/'build/runs') as root:
            run(args.executable,Path(root)/'captures',args.expected_validation,args.case)
