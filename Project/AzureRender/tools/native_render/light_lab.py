"""Generate and measure a public, deterministic local-light GPU fixture."""
import argparse
import base64
import json
import math
from pathlib import Path
import struct
import subprocess
import re


def generate(root, mode):
    root.mkdir(parents=True, exist_ok=False)
    def write(name, data):
        (root / name).write_text(json.dumps(data, indent=2), encoding='utf-8')
    positions = [-4,-4,0, 4,-4,0, 4,4,0, -4,4,0]
    blob = struct.pack('<12f12f8f6H', *positions, *([0,0,1]*4),
                       0,0,1,0,1,1,0,1, 0,1,2,0,2,3)
    write('plane.gltf', {'asset': {'version': '2.0'}, 'scene': 0,
        'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}],
        'buffers': [{'byteLength': len(blob), 'uri': 'data:application/octet-stream;base64,'+base64.b64encode(blob).decode()}],
        'bufferViews': [{'buffer': 0, 'byteOffset': o, 'byteLength': n} for o,n in [(0,48),(48,48),(96,32),(128,12)]],
        'accessors': [{'bufferView': i, 'componentType': 5126 if i<3 else 5123,
            'count': 4 if i<3 else 6, 'type': ['VEC3','VEC3','VEC2','SCALAR'][i],
            **({'min': [-4,-4,0], 'max': [4,4,0]} if i==0 else {})} for i in range(4)],
        'meshes': [{'primitives': [{'attributes': {'POSITION':0,'NORMAL':1,'TEXCOORD_0':2}, 'indices':3,'material':0}]}],
        'materials': [{'pbrMetallicRoughness': {'baseColorFactor':[1,1,1,1], 'metallicFactor':0,'roughnessFactor':1}}]})
    def component(kind, data): return {'type':kind,'version':1,'data':data}
    nodes = [{'id':'plane','resourceId':'plane'}, {'id':'camera','components':{
        'azure.transform':component('azure.transform',{'translation':[0,0,5]}),
        'azure.camera':component('azure.camera',{'orthographic':True,'orthographicHeight':8})}},
        {'id':'light','components':{'azure.transform':component('azure.transform',{'translation':[0,0,2]})}}]
    lights = [] if mode=='dark' else [{'id':'light','nodeId':'light','intensity':3,'radius':10,
        'shape':{'type':'spot' if mode=='spot' else 'point','direction':[0,0,-1],'innerCone':.25,'outerCone':.5}}]
    if mode in ('smooth-inverse-square','range-power'):
        lights[0]['intensity']=.3
        lights[0]['shape'].update(falloff=mode,falloffExponent=3.5)
    if mode.startswith('directional-'):
        lights[0]['intensity']=.3
        lights[0]['radius']=.001
        lights[0]['shape'].update(type='directional',falloff='range-power',falloffExponent=3.5)
        if mode=='directional-far':
            nodes[2]['components']['azure.transform']['data']['translation']=[10000,10000,-10000]
        elif mode=='directional-back':lights[0]['shape']['direction']=[0,0,1]
    write('level.azurelevel', {'schemaVersion':1,'id':'light-lab','resources':[{'id':'plane','asset':'assets:/plane.gltf'}],
        'nodes':nodes,'lights':lights,'renderSettings':{'sceneLighting':True,'skyIntensity':0,
        'stylized':False,'background':False,'platform':False,'innerOutline':False,'silhouetteOutline':False,
        'antiAliasing':0,'bloomEnabled':False,'toneMapping':False,'outline':0}})
    write('lab.azureproject', {'schemaVersion':1,'id':'light-lab','name':'Local light laboratory',
        'mounts':[{'name':'assets','path':'.'}],'startupScene':'assets:/level.azurelevel'})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--player', type=Path, required=True)
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    from PIL import Image
    output = args.output.resolve()
    metrics = {}
    pictures = {}
    for mode in ('dark','point','spot','smooth-inverse-square','range-power',
                 'directional-near','directional-far','directional-back'):
        folder = output / mode
        if not args.verify_only:
            generate(folder, mode)
        command = [str(args.player.resolve()), '--project', str(folder/'lab.azureproject'),
            '--width','256','--height','256','--capture-dir',str(folder/'frames'),
            '--capture-frames','1','--capture-fps','30','--fixed-frame-step',
            '--runtime-report',str(folder/'runtime.json')]
        if not args.verify_only:
            with (folder/'player.log').open('w', encoding='utf-8') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
        log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
        assert not re.search(r'VUID-|Validation Error|\[error\]',log),mode
        assert 'Allocator after unload: buffers=0 images=0' in log,mode
        runtime=json.loads((folder/'runtime.json').read_text())
        assert not runtime['scriptErrors'] and not runtime['presentationErrors'],mode
        picture = Image.open(folder/'frames/frame_000000.png').convert('RGB')
        pictures[mode] = picture
        metrics[mode] = {'center':picture.getpixel((128,128)), 'corner':picture.getpixel((40,40)),
                         'maximum':max(high for low,high in picture.crop((8,8,248,248)).getextrema())}
        if mode=='spot':
            row = [max(picture.getpixel((x,128))) for x in range(256)]
            lit = [x for x,v in enumerate(row) if v>1]
            metrics[mode]['horizontal_extent'] = [min(lit), max(lit)] if lit else None
            metrics[mode]['geometric_radius_pixels'] = math.tan(.5)*2/8*256
    def linear(byte):
        v=byte/255
        return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
    errors=[]
    for x in (140,145,150,155):
        distance_squared=(((x+.5)/256*8)-4)**2+(((128+.5)/256*8)-4)**2
        weight=max(0,min(1,(2/math.sqrt(4+distance_squared)-math.cos(.5))/(math.cos(.25)-math.cos(.5))))**2
        measured=linear(pictures['spot'].getpixel((x,128))[0])/linear(pictures['point'].getpixel((x,128))[0])
        errors.append(abs(measured-weight)/weight)
    metrics['cone_relative_errors']=errors
    extent=metrics['spot']['horizontal_extent']
    metrics['boundary_error_pixels']=max(abs(abs(x-127.5)-metrics['spot']['geometric_radius_pixels']) for x in extent) if extent else None
    (output/'metrics.json').write_text(json.dumps(metrics,indent=2))
    assert metrics['dark']['maximum']==0, metrics
    assert max(metrics['point']['corner'])>0, metrics
    assert max(metrics['spot']['center'])>0 and max(metrics['spot']['corner'])==0, metrics
    assert max(errors)<.01, metrics
    assert metrics['boundary_error_pixels']<=1, metrics
    distance_errors={}
    for model in ('smooth-inverse-square','range-power'):
        distance_errors[model]=[]
        for x in (128,150,175,205):
            squared=4+(((x+.5)/256*8)-4)**2+(((128+.5)/256*8)-4)**2
            legacy=(1-math.sqrt(squared)/10)**2/(1+squared*.12)
            target=(1-(squared/100)**2)**2/(squared+1) if model=='smooth-inverse-square' else (1-squared/100)**3.5
            measured=10*linear(pictures[model].getpixel((x,128))[0])/linear(pictures['point'].getpixel((x,128))[0])
            distance_errors[model].append(abs(measured-target/legacy)/(target/legacy))
        assert max(distance_errors[model])<.03,(model,distance_errors[model])
    metrics['distance_relative_errors']=distance_errors
    assert max(pictures['directional-near'].getpixel((128,128)))>0
    assert pictures['directional-near'].tobytes()==pictures['directional-far'].tobytes(), 'Directional illumination depends on position or radius'
    assert metrics['directional-back']['maximum']==0,'Directional emission has the wrong sign'
    metrics['directional_translation_byte_difference']=0
    (output/'metrics.json').write_text(json.dumps(metrics,indent=2))
    print(json.dumps(metrics,indent=2))


if __name__ == '__main__': main()
