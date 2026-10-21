"""Compare production spatial AA with an independent 4x raster reference."""
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
from create_visibility_fixture import component


def fixture(path):
    """CC0 sloped silhouettes, subpixel strips and HDR emissive contrasts."""
    positions=[];indices=[];primitives=[];materials=[]
    for group,(cy,intensity) in enumerate(((.82,1.),(-.82,12.))):
        begin=len(indices);material=len(materials)
        materials.append({'doubleSided':True,'pbrMetallicRoughness':{
            'baseColorFactor':[.0,.0,.0,1],'roughnessFactor':1,'metallicFactor':0},
            'emissiveFactor':[intensity,intensity*.8,intensity*.5]})
        for index in range(11):
            cx=-2.3+index*.46;angle=math.radians(8+index*7.1)
            width=.025+(index%4)*.022;height=1.22
            points=[]
            for x,y in ((-width/2,-height/2),(width/2,-height/2),(width/2,height/2),(-width/2,height/2)):
                points.append((cx+x*math.cos(angle)-y*math.sin(angle),cy+x*math.sin(angle)+y*math.cos(angle),0))
            start=len(positions);positions.extend(points);indices.extend(start+v for v in (0,1,2,0,2,3))
        primitives.append((begin,len(indices)-begin,material))
    vertex_bytes=struct.pack('<'+'f'*(len(positions)*3),*(v for p in positions for v in p))
    normals=struct.pack('<'+'f'*(len(positions)*3),*([0,0,1]*len(positions)))
    binary=vertex_bytes+normals+struct.pack('<'+'H'*len(indices),*indices)
    accessors=[{'bufferView':0,'componentType':5126,'count':len(positions),'type':'VEC3',
                'min':np.min(positions,axis=0).tolist(),'max':np.max(positions,axis=0).tolist()},
               {'bufferView':1,'componentType':5126,'count':len(positions),'type':'VEC3'}]
    views=[{'buffer':0,'byteOffset':0,'byteLength':len(vertex_bytes)},
           {'buffer':0,'byteOffset':len(vertex_bytes),'byteLength':len(normals)}]
    meshes=[]
    for begin,count,material in primitives:
        views.append({'buffer':0,'byteOffset':len(vertex_bytes)+len(normals)+begin*2,'byteLength':count*2})
        accessors.append({'bufferView':len(views)-1,'componentType':5123,'count':count,'type':'SCALAR'})
        meshes.append({'attributes':{'POSITION':0,'NORMAL':1},'indices':len(accessors)-1,'material':material})
    path.write_text(json.dumps({'asset':{'version':'2.0','generator':'Azure spatial-AA fixture',
        'extras':{'license':'CC0-1.0'}},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'mesh':0}],
        'buffers':[{'byteLength':len(binary),'uri':'data:application/octet-stream;base64,'+base64.b64encode(binary).decode()}],
        'bufferViews':views,'accessors':accessors,'materials':materials,'meshes':[{'primitives':meshes}]}))


def read_linear(path):
    value=np.asarray(Image.open(path).convert('RGB'),dtype=np.float64)/255
    return np.where(value<=.04045,value/12.92,((value+.055)/1.055)**2.4)


def edge_mask(reference):
    luma=reference@np.array([.2126,.7152,.0722])
    gradient=np.maximum(np.abs(np.diff(luma,axis=0,append=luma[-1:])),
                        np.abs(np.diff(luma,axis=1,append=luma[:,-1:])))
    seed=gradient>.025
    # Include both sides of an edge, with a guard for static flat-region checks.
    padded=np.pad(seed,2);mask=np.zeros_like(seed)
    for y in range(5):
        for x in range(5):mask|=padded[y:y+seed.shape[0],x:x+seed.shape[1]]
    return mask


def encoded(value):
    return np.where(value<=.0031308,value*12.92,1.055*np.maximum(value,0)**(1/2.4)-.055)


def run(executable,output,phases=8,auxiliary=False):
    output.mkdir(parents=True,exist_ok=True);assets=output/'assets';assets.mkdir(exist_ok=True)
    fixture(assets/'edges.gltf')
    base={'schemaVersion':1,'id':'spatial-aa','sceneType':'character','resources':[{'id':'edges','asset':'assets:/edges.gltf'}],
        'renderSettings':{'platform':False,'innerOutline':False,'silhouetteOutline':False,'bloomEnabled':False,
            'antiAliasing':0,'exposure':0,'saturation':1,'contrast':1,'toneMapping':True},
        'nodes':[{'id':'edges','resourceId':'edges'},{'id':'camera','components':{
            'azure.transform':component('azure.transform',{'translation':[0,0,5]}),
            'azure.camera':component('azure.camera',{'enabled':True,'priority':1,'fieldOfView':55})}}]}
    project=output/'project.azureproject';project.write_text(json.dumps({'schemaVersion':1,'id':'spatial-aa','name':'Spatial AA fixture',
        'mounts':[{'name':'assets','path':'assets'}],'startupScene':'assets:/scene.azurelevel'}))
    width,height=320,180;frames={};case_results=[]
    for phase in range(phases):
        shift=(phase/max(phases,1))*2*math.tan(math.radians(55/2))*5/height
        for mode,scale in [('off',1),('edge',1),('reference',4)]+([('supersample',1)] if phase==0 else []):
            folder=output/f'{phase:02d}-{mode}';folder.mkdir(exist_ok=True)
            level=copy.deepcopy(base);level['renderSettings']['antiAliasing']={'off':0,'reference':0,'edge':1,'supersample':2}[mode]
            level['nodes'][1]['components']['azure.transform']['data']['translation'][0]=shift
            (assets/'scene.azurelevel').write_text(json.dumps(level))
            command=[str(executable.resolve()),'--project',str(project.resolve()),'--width',str(width*scale),'--height',str(height*scale),
                '--qa-camera','scene','--qa-light','scene','--capture-frames','1','--capture-dir',str(folder/'frames'),'--fixed-frame-step','--gpu-timing']
            if auxiliary:
                command[1:3]=['--asset',str((assets/'edges.gltf').resolve())]
                descriptor=folder/'views.json';descriptor.write_text(json.dumps({'schemaVersion':1,'views':[{'renderer':'character',
                    'extent':[width*scale,height*scale],'position':[shift,1,3.5],'target':[shift,1,0],'transfer':'srgb',
                    'antiAliasing':level['renderSettings']['antiAliasing']}]}))
                command.extend(['--preview-views',str(descriptor),'--runtime-report',str(folder/'runtime.json')])
            with (folder/'run.log').open('w',encoding='utf8') as log:
                subprocess.run(command,cwd=folder,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(folder/'run.log').read_text(encoding='utf8',errors='replace')
            assert not re.search(r'VUID-|Validation Error|\[error\]',log),folder
            assert 'Allocator after unload: buffers=0 images=0' in log,folder
            image=folder/('preview-images/0.png' if auxiliary else 'frames/frame_000000.png')
            frames[phase,mode]=read_linear(image)
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text())
            if not auxiliary:assert manifest['antiAliasing']==level['renderSettings']['antiAliasing'],folder
            scene_extent=[manifest['sceneRenderWidth'],manifest['sceneRenderHeight']]
            if auxiliary:
                view=json.loads((folder/'runtime.json').read_text())['developer']['views'][0]
                assert view['antiAliasing']==level['renderSettings']['antiAliasing'],folder
                scene_extent=view['sceneExtent']
                factor=2 if mode=='supersample' else 1
                assert scene_extent==[width*scale*factor,height*scale*factor],folder
            case_results.append({'phase':phase,'mode':mode,'outputExtent':[width*scale,height*scale],
                'sceneExtent':scene_extent})
            print(phase,mode,'captured',flush=True)
    errors={'off':[],'edge':[]};flat=[];integrals=[];references=[];masks=[]
    for phase in range(phases):
        reference=frames[phase,'reference'].reshape(height,4,width,4,3).mean((1,3));references.append(reference)
        mask=edge_mask(reference);masks.append(mask)
        assert mask.sum()>500,'Fixture must expose enough high-contrast edge pixels'
        for mode in errors:errors[mode].append(float(np.abs(frames[phase,mode]-reference)[mask].mean()))
        flat_mask=~mask
        flat.append(float(np.abs(encoded(frames[phase,'edge'])-encoded(frames[phase,'off']))[flat_mask].max()*255))
        integrals.append({mode:float(abs(frames[phase,mode].sum()-reference.sum())/reference.sum()) for mode in errors})
    temporal={mode:[] for mode in errors}
    for phase in range(1,phases):
        mask=masks[phase]|masks[phase-1];delta=references[phase]-references[phase-1]
        for mode in errors:
            residual=(frames[phase,mode]-frames[phase-1,mode])-delta
            temporal[mode].append(float(np.abs(residual)[mask].mean()))
    summary={'schemaVersion':1,'executableSha256':hashlib.sha256(executable.read_bytes()).hexdigest(),
        'referenceScale':4,'outputExtent':[width,height],'auxiliary':auxiliary,'cases':case_results,
        'edgeMeanAbsoluteErrors':errors,'edgeErrorRatio':float(np.mean(errors['edge'])/np.mean(errors['off'])),
        'temporalMeanAbsoluteErrors':temporal,'flatMaximumByteDeltas':flat,'integralRelativeErrors':integrals}
    if temporal['off']:summary['temporalErrorRatio']=float(np.mean(temporal['edge'])/np.mean(temporal['off']))
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary),flush=True)
    assert summary['edgeErrorRatio']<=.8,'Native-resolution AA must reduce independent edge error by at least 20%'
    assert max(flat)<=1.001,'AA must preserve flat regions within one encoded byte'
    assert max(item['edge'] for item in integrals)<=.02,'Thin features must preserve integral brightness within 2% of the reference'
    if temporal['off']:assert summary['temporalErrorRatio']<1,'AA must reduce motion-difference error'


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',required=True,type=Path)
    parser.add_argument('--output',type=Path);parser.add_argument('--phases',type=int,default=8)
    parser.add_argument('--auxiliary',action='store_true');args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.phases,args.auxiliary)
    else:
        with tempfile.TemporaryDirectory(prefix='spatial-aa-',dir=Path(__file__).resolve().parents[1]/'build/runs') as output:
            run(args.executable,Path(output),args.phases,args.auxiliary)
