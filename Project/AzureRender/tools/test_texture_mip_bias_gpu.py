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


def authored_mips():
    colors=((255,0,0,255),(0,255,0,255),(0,0,255,255),(255,255,0,255),
            (255,0,255,255),(0,255,255,255),(128,128,128,255),(255,255,255,255))
    words=[124,0x2100f,128,128,512,0,8]+[0]*11
    words += [32,0x41,0,32,255,65280,16711680,4278190080,0x401008,0,0,0,0]
    return b'DDS '+struct.pack('<31I',*words)+b''.join(bytes(color)*(128>>i)**2 for i,color in enumerate(colors))


def run(player,output,expected_validation='any',selected=None):
    output.mkdir(parents=True,exist_ok=False)
    names=('neutral','biased','fixed','documents','device-clamped','mip-filter')
    if selected:
        assert set(selected)<=set(names);names=tuple(name for name in names if name in selected)
    report={'schemaVersion':1,'status':'running','captures':[],
            'scope':'Implicit LOD 2 from 128 texels over 32 pixels; authored mip colors and public image metadata',
            'playerSha256':hashlib.sha256(player.read_bytes()).hexdigest()}
    images={};statistics={}
    try:
        for name in names:
            print(name,flush=True)
            root=output/name;generate(root,'dark')
            model=json.loads((root/'plane.gltf').read_text(encoding='utf-8'))
            model['materials'][0]['name']='MipPanel'
            model['materials'][0]['pbrMetallicRoughness']['baseColorTexture']={'index':0}
            model['textures']=[{'source':0}]
            nodes=[];resources=[]
            for index,side in enumerate(('left','right')):
                identity=str(uuid.uuid4());bias=0 if name in ('neutral','mip-filter') else ((-16 if index==0 else 16) if name=='device-clamped' else (-1 if index==0 else 1))
                path=root/(side+'.dds');path.write_bytes(authored_mips())
                metadata={'schemaVersion':1,'id':identity,'dependencies':[],
                          'settings':{'texture':{'filter':'linear','mipFilter':'linear' if name=='mip-filter' and index==1 else 'nearest','anisotropy':1,'mipBias':bias}}}
                Path(str(path)+'.azmeta').write_text(json.dumps(metadata),encoding='utf-8')
                asset=copy.deepcopy(model);asset['images']=[{'uri':side+'.dds'}]
                (root/(side+'.gltf')).write_text(json.dumps(asset),encoding='utf-8')
                resources.append({'id':side,'asset':'assets:/'+side+'.gltf'})
                node={'id':side,'resourceId':side,'components':{'azure.transform':{
                    'type':'azure.transform','version':1,'data':{'translation':[-9 if index==0 else 9,0,0]}}}}
                if name=='documents':
                    document=complete_material_document(asset['materials'][0],asset,{'0':identity})
                    material=root/(side+'.azurematerial');material.write_text(json.dumps(document),encoding='utf-8')
                    material_id=str(uuid.uuid4())
                    Path(str(material)+'.azmeta').write_text(json.dumps({'schemaVersion':1,'id':material_id,'settings':{},'dependencies':[]}),encoding='utf-8')
                    node['materialSlots']={'name:MipPanel':material_id}
                nodes.append(node)
            path=root/'level.azurelevel';level=json.loads(path.read_text(encoding='utf-8'))
            camera=next(n for n in level['nodes'] if n['id']=='camera')
            camera['components']['azure.camera']['data']['orthographicHeight']=32*2**.25 if name=='mip-filter' else 32
            level.update(resources=resources,nodes=nodes+[camera],lights=[])
            path.write_text(json.dumps(level),encoding='utf-8')
            command=[str(player.resolve()),'--project',str((root/'lab.azureproject').resolve()),
                     '--width','128','--height','128','--capture-dir',str(root/'frames'),
                     '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene',
                     '--qa-isolation','albedo','--runtime-report',str(root/'runtime.json')]
            if name=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
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
            expected=np.array([(0,0,255),(0,0,255)] if name=='neutral' else [(0,255,0),(255,255,0)],dtype=int)
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
