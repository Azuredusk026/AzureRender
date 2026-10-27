"""Exercise public clipmap scene bindings with independent analytic irradiance."""
import argparse
import base64
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile

try:
    from .light_lab import generate
except ImportError:
    from light_lab import generate


def field(path, mode):
    transform=[[1,0,0,0],[0,1,0,0],[0,0,1,0]]
    if mode=='mirrored':transform[0][0]=-1
    if mode=='translated':transform[0][3]=.25
    if mode=='outside':transform[0][3]=500
    if mode=='fine-transition':transform[0][3]=30
    if mode=='medium-transition':transform[0][3]=120
    if mode=='outer-transition':transform[0][3]=480
    rows=transform+[[0,0,0,.1],[0,0,0,.2],[0,0,0,.3]]
    for layer in range(3):rows.extend([[0,0,0,2**layer],[29*4**layer,13*4**layer,2*4**layer,0]])
    payload=b'AZIC0001'+struct.pack('<4I',2,2,2,mode!='disabled-stack')
    payload+=struct.pack('<48f',*(f for row in rows for f in row))+b'\0'*8
    assert len(payload)==224
    colors=((255,128,0,0),(0,255,128,128),(128,0,255,255))
    for layer in range(3):
        for z in range(2):
            for y in range(2):
                for x in range(2):
                    exponent=layer+(1 if layer==0 and x else 0)
                    packed=((15+exponent)<<6)|(((16+exponent)<<6)<<11)|(((17+exponent)<<5)<<22)
                    payload+=struct.pack('<I',packed)
        payload+=bytes(channel for z in range(2) for color in colors for y in range(2)
                       for x in range(2) for channel in color)
    path.write_bytes(payload)


def expected(mode, x):
    if mode=='disabled-binding':return [.12,.08,.04]
    intensity=1/128 if mode=='half-intensity' else 1/64
    if mode in ('disabled-stack','outside'):return [v*intensity for v in (.1,.2,.3)]
    if mode=='medium-transition':
        fade=max(0,min(1,(120+x-116)/8));magnitude=2+2*fade
        return [0,2*(1+2/255)*magnitude*intensity,12*magnitude*intensity]
    if mode=='outer-transition':
        fade=max(0,min(1,(480+x-464)/32));remapped=max(0,min(1,2*fade-1));default=.5*(remapped+fade)
        return [0,((1-fade)*8*(1+2/255)+default*.2)*intensity,((1-fade)*48+default*.3)*intensity]
    coordinate=-x if mode=='mirrored' else x+.25 if mode=='translated' else x
    if mode=='fine-transition':coordinate+=30
    lower=math.floor(coordinate);fraction=coordinate-lower
    magnitude=2-fraction if lower%2 else 1+fraction
    if mode=='fine-transition':
        fade=max(0,min(1,(coordinate-29)/2));magnitude=(1-fade)*magnitude+fade*2
    incident=[0,6,4*(1-2/255)] if mode=='mirrored' else [0,2*(1+2/255),12]
    return [v*magnitude*intensity for v in incident]


def run(player, output):
    from PIL import Image
    output.mkdir(parents=True,exist_ok=False)
    results={}
    modes=('baseline','half-intensity','disabled-stack','disabled-binding','outside',
           'translated','fine-transition','medium-transition','outer-transition','mirrored','legacy','msaa4','snapshot')
    for mode in modes:
        folder=output/mode;project=folder/'project';generate(project,'dark')
        field(project/'field.bin',mode)
        environment=(0,0,0,1) if mode=='snapshot' else (.12,.08,.04,1)
        (project/'global.azenv').write_bytes(b'AZENV001'+struct.pack('<III',2,1,1)+struct.pack('<8e',*(environment*2)))
        level=json.loads((project/'level.azurelevel').read_text())
        level['renderSettings']['skyIntensity']=1
        level['irradianceVolume']=dict(version=1,asset='assets:/field.bin',enabled=mode!='disabled-binding',
                                       intensity=1/128 if mode=='half-intensity' else 1/64)
        if mode in ('msaa4','snapshot'):level['renderSettings']['schemaVersion']=19
        if mode=='msaa4':level['renderSettings']['antiAliasing']=4
        if mode=='snapshot':level['renderSettings']['screenSpaceReflection']=dict(schemaVersion=1,enabled=True,maxSteps=32)
        (project/'level.azurelevel').write_text(json.dumps(level,indent=2))
        if mode=='mirrored':
            gltf=json.loads((project/'plane.gltf').read_text());prefix='data:application/octet-stream;base64,'
            raw=bytearray(base64.b64decode(gltf['buffers'][0]['uri'][len(prefix):]))
            struct.pack_into('<12f',raw,48,*([1,0,0]*4));gltf['buffers'][0]['uri']=prefix+base64.b64encode(raw).decode()
            (project/'plane.gltf').write_text(json.dumps(gltf))
        command=[str(player.resolve()),'--project',str(project/'lab.azureproject'),'--environment',str(project/'global.azenv'),
            '--width','128','--height','128','--fixed-frame-step','--capture-dir',str(folder/'frames'),'--capture-frames','1',
            '--runtime-report',str(folder/'runtime.json'),'--gpu-timing-output',str(folder/'gpu.json'),
            '--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty' if mode=='snapshot' else 'ambient']
        if mode=='legacy':command.append('--disable-bindless')
        (folder/'command.json').write_text(json.dumps(command,indent=2))
        startup=None
        if hasattr(subprocess,'STARTUPINFO'):
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        with (folder/'player.log').open('w') as log:
            subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=180,check=True,startupinfo=startup)
        log=(folder/'player.log').read_text(errors='replace')
        assert 'VUID-' not in log and 'Validation Error' not in log,mode
        assert 'Allocator after unload: buffers=0 images=0' in log,mode
        report=json.loads((folder/'runtime.json').read_text());assert not report['presentationErrors'] and not report['scriptErrors'],mode
        gpu=json.loads((folder/'gpu.json').read_text());assert gpu['irradianceResourceKind']=='clipmap',mode
        picture=Image.open(folder/'frames/frame_000000.png').convert('RGB');measurements=[]
        for x,y in ((64,64),(40,40),(88,88)):
            pixel=picture.getpixel((x,y));linear=[v/255/12.92 if v/255<=.04045 else ((v/255+.055)/1.055)**2.4 for v in pixel]
            truth=expected(mode,(x+.5)/128*8-4)
            measurements.append(dict(pixel=list(pixel),linear=linear,expected=truth,error=max(abs(a-b) for a,b in zip(linear,truth))))
        results[mode]=dict(measurements=measurements,resourceKind=gpu['irradianceResourceKind'],
                           validationErrors=0,resourcesReleased=True)
        (output/'summary.json').write_text(json.dumps(results,indent=2))
        assert max(m['error'] for m in measurements)<.012,(mode,measurements)
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player',type=Path,required=True);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:results=run(args.player,args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='azure-clipmap-scene-') as temporary:
            results=run(args.player,Path(temporary)/'result')
    print(json.dumps({k:max(m['error'] for m in v['measurements']) for k,v in results.items()}))
