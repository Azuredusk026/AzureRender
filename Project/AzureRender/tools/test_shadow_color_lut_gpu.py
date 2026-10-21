"""Compare the real material lookup with independent cube interpolation."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import surface,component


def linear(encoded):
    encoded=np.asarray(encoded,dtype=float)
    return np.where(encoded<=.04045,encoded/12.92,((encoded+.055)/1.055)**2.4)


def srgb(value):
    value=np.maximum(value,0)
    return np.where(value<=.0031308,value*12.92,1.055*value**(1/2.4)-.055)


def expected_lookup(pixels,color):
    # Evaluate all eight cube vertices. This does not use packed 2D sampling
    # coordinates or the shader's two bilinear-fetch implementation.
    dimension=pixels.shape[0]
    coordinate=np.rint(np.clip(srgb(np.asarray(color)),0,1)*255)/255*(dimension-1)
    first=np.floor(coordinate).astype(int);fraction=coordinate-first
    result=np.zeros(3)
    for red in (0,1):
        for green in (0,1):
            for blue in (0,1):
                offset=np.array([red,green,blue])
                vertex=np.minimum(first+offset,dimension-1)
                weight=np.prod(np.where(offset,fraction,1-fraction))
                result+=linear(pixels[vertex[1],vertex[2]*dimension+vertex[0],:3]/255)*weight
    return np.rint(np.clip(srgb(result),0,1)*255)


def run(player,root,require_validation=False):
    root.mkdir(parents=True,exist_ok=True)
    assets=root/'assets';assets.mkdir(exist_ok=False)
    surface(assets/'surface.gltf',double_sided=True)
    model=json.loads((assets/'surface.gltf').read_text())
    dimension=4;pixels=np.zeros((dimension,dimension**2,4),dtype=np.uint8)
    for green in range(dimension):
        for blue in range(dimension):
            for red in range(dimension):pixels[green,blue*dimension+red]=[red*85,green*85,blue*85,255]
    Image.fromarray(pixels,'RGBA').save(assets/'lookup.png')
    Image.fromarray(pixels[::-1],'RGBA').save(assets/'flipped.png')
    red_pixels=np.zeros_like(pixels);red_pixels[...,0]=red_pixels[...,3]=255
    Image.fromarray(red_pixels,'RGBA').save(assets/'red.png')
    model['images']=[{'uri':'lookup.png'}];model['textures']=[{'source':0}]
    profile={'schemaVersion':2,'class':'generic','features':[],
        'shadowColorLut':{'schemaVersion':1,'texture':0,'flipV':False}}
    model['materials'][0]['extras']={'azureRenderMaterial':profile}
    settings={'platform':False,'silhouetteOutline':False,'innerOutline':False,
        'bloomEnabled':False,'antiAliasing':0,'exposure':0,'saturation':1,'contrast':1,
        'toneMapping':False,'skyIntensity':0,'sceneLighting':True}
    level={'schemaVersion':1,'id':'lut-lab','sceneType':'character','renderSettings':settings,
        'resources':[{'id':'surface','asset':'assets:/surface.gltf'}],
        'nodes':[{'id':'surface','resourceId':'surface'},
            {'id':'camera','components':{'azure.transform':component('azure.transform',{'translation':[0,0,3]}),
                'azure.camera':component('azure.camera',{'enabled':True,'priority':1,'fieldOfView':55})}},
            {'id':'light','components':{'azure.transform':component('azure.transform',{'translation':[0,0,2]})}}],
        'lights':[{'id':'light','nodeId':'light','color':[1,1,1],'intensity':8,'radius':10,'enabled':True}]}
    (assets/'scene.azurelevel').write_text(json.dumps(level))
    project=root/'project.azureproject'
    project.write_text(json.dumps({'schemaVersion':1,'id':'lut-lab','name':'Color cube lookup',
        'mounts':[{'name':'assets','path':'assets'}],'startupScene':'assets:/scene.azurelevel'}))
    metrics={};frames={};validation_enabled={}
    cases=[('identity',[.18,.04,.7]),('toe',[.0005,.003,.01]),('black',[0,0,0]),('white',[1,1,1]),
        ('flipped',[.18,.04,.7]),('constant',[.18,.04,.7]),('fixed',[.18,.04,.7]),('disabled',[.18,.04,.7]),
        ('beauty-enabled',[.18,.04,.7]),('beauty-disabled',[.18,.04,.7])]
    for mode,color in cases:
        model['materials'][0]['pbrMetallicRoughness']['baseColorFactor']=[*color,1]
        model['images'][0]['uri']='flipped.png' if mode=='flipped' else ('red.png' if mode=='constant' else 'lookup.png')
        profile['shadowColorLut']['flipV']=mode=='flipped'
        active=copy.deepcopy(profile)
        if mode in ('disabled','beauty-disabled'):active.pop('shadowColorLut')
        model['materials'][0]['extras']={'azureRenderMaterial':active}
        (assets/'surface.gltf').write_text(json.dumps(model))
        folder=root/mode;folder.mkdir()
        command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128',
            '--capture-dir',str(folder/'frames'),'--capture-frames','1','--fixed-frame-step',
            '--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty' if mode.startswith('beauty') else 'shadow-color',
            '--runtime-report',str(folder/'runtime.json')]
        if mode=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
        (folder/'command.json').write_text(json.dumps(command,indent=2))
        with (folder/'player.log').open('w',encoding='utf8') as log:
            subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
        text=(folder/'player.log').read_text(encoding='utf8',errors='replace')
        assert not re.search(r'VUID-|Validation Error|\[error\]',text),mode
        validation_enabled[mode]='Validation layer: enabled' in text
        if require_validation:assert validation_enabled[mode],mode
        assert 'Allocator after unload: buffers=0 images=0' in text,mode
        report=json.loads((folder/'runtime.json').read_text())
        assert not report['scriptErrors'] and not report['presentationErrors'],mode
        frame=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'),dtype=float)
        frames[mode]=frame
        pixel=frame[64,64]
        if mode.startswith('beauty'):continue
        expected=np.zeros(3) if mode=='disabled' else expected_lookup(red_pixels if mode=='constant' else pixels,color)
        error=np.abs(pixel-expected)
        assert error.max()<=1,(mode,pixel.tolist(),expected.tolist(),error.tolist())
        metrics[mode]={'actual':pixel.tolist(),'expected':expected.tolist(),'maximumByteError':float(error.max())}
    assert np.array_equal(frames['identity'],frames['fixed']),'Indexed and fixed lookup differ'
    assert np.array_equal(frames['identity'],frames['flipped']),'Explicit orientation normalization differs'
    assert np.array_equal(frames['beauty-enabled'],frames['beauty-disabled']),'Lookup binding must not imply an unverified NPR composition'
    summary={'playerSha256':hashlib.sha256(player.read_bytes()).hexdigest(),'frames':len(cases),'validationErrors':0,
        'validationRequired':require_validation,'validationEnabled':validation_enabled,
        'scope':'Real material GPU lookup; full native NPR composition and linear HDR 1% accuracy remain pending','byteTolerance':1,'cases':metrics,
        'fixedDifference':0,'flipDifference':0,'beautyDifference':0}
    (root/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--require-validation',action='store_true')
    args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.require_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='shadow-lut-',dir=Path(__file__).resolve().parents[1]/'build/runs') as folder:run(args.executable,Path(folder),args.require_validation)
