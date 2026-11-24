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
    if mode=='receiver-grid':
        payload=struct.pack('<8s4f3I',b'AZIV0001',-4,-4,-4,8,2,2,2)
        for z in range(2):
            for y in range(2):
                for x in range(2):
                    payload+=struct.pack('<16f',0,0,0,.05+.5*x+z,0,0,0,.05+.05*y,
                                         0,0,0,.1+.05*z,1,0,0,0)
        path.write_bytes(payload)
        return
    transform=[[1,0,0,0],[0,1,0,0],[0,0,1,0]]
    if mode=='mirrored' or mode in FRAME_FIELD:transform[0][0]=-1
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


MODES=('baseline','half-intensity','disabled-stack','disabled-binding','outside',
       'translated','fine-transition','medium-transition','outer-transition','mirrored','legacy','msaa4','snapshot',
       'receiver-offset','receiver-jitter','receiver-combined','receiver-grid','receiver-control','receiver-snapshot',
       'receiver-npr-control','receiver-npr','receiver-frame-pixel','receiver-frame-field',
       'receiver-frame-combined','receiver-npr-frame')
FRAME_FIELD=('receiver-frame-field','receiver-frame-combined')
FRAME_BOTTOM=('receiver-frame-pixel','receiver-frame-combined','receiver-npr-frame')
NPR=('receiver-npr-control','receiver-npr','receiver-npr-frame')


def expected(mode, x, pixel_x):
    if mode in NPR:
        return [value*.95999997854232788 for value in expected('baseline',x,pixel_x)]
    if mode=='disabled-binding':return [.12,.08,.04]
    intensity=1/16 if mode in ('receiver-offset','receiver-combined','receiver-control')+FRAME_FIELD else 1/128 if mode=='half-intensity' else 1/64
    if mode in ('disabled-stack','outside'):return [v*intensity for v in (.1,.2,.3)]
    if mode=='medium-transition':
        fade=max(0,min(1,(120+x-116)/8));magnitude=2+2*fade
        return [0,2*(1+2/255)*magnitude*intensity,12*magnitude*intensity]
    if mode=='outer-transition':
        fade=max(0,min(1,(480+x-464)/32));remapped=max(0,min(1,2*fade-1));default=.5*(remapped+fade)
        return [0,((1-fade)*8*(1+2/255)+default*.2)*intensity,((1-fade)*48+default*.3)*intensity]
    coordinate=-x if mode=='mirrored' or mode in FRAME_FIELD else x+.25 if mode=='translated' else x
    if mode.startswith('receiver-'):
        # Lossless D3D11 diagnostic readback from active source event 8912, phase 49.
        native_noise={64:-.5193634033203125,40:-.072666168212890625,88:-.9319000244140625}
        coordinate+=.25 if mode in ('receiver-offset','receiver-combined') else 0
        coordinate-=.25 if mode in FRAME_FIELD else 0
        coordinate+=.2*native_noise[pixel_x] if mode in ('receiver-jitter','receiver-combined','receiver-snapshot','receiver-frame-pixel')+FRAME_FIELD else 0
        if mode=='receiver-grid':
            noise=.2*native_noise[pixel_x]
            qx=(x+noise+4)/8;qy=(-x+noise+4)/8;qz=(.25+noise+4)/8
            return [.05+.5*qx+qz,.05+.05*qy,.1+.05*qz]
    if mode=='fine-transition':coordinate+=30
    lower=math.floor(coordinate);fraction=coordinate-lower
    magnitude=2-fraction if lower%2 else 1+fraction
    if mode=='fine-transition':
        fade=max(0,min(1,(coordinate-29)/2));magnitude=(1-fade)*magnitude+fade*2
    incident=[0,6,4*(1-2/255)] if mode=='mirrored' or mode in FRAME_FIELD else [0,2*(1+2/255),12]
    if mode in ('receiver-offset','receiver-combined','receiver-control'):incident=[3,0,4*(1+2/255)]
    return [v*magnitude*intensity for v in incident]


def run(player, output, selected=None, fixed=False):
    from PIL import Image
    output.mkdir(parents=True,exist_ok=False)
    results={}
    modes=('receiver-npr-control',selected) if selected in ('receiver-npr','receiver-npr-frame') else (selected,) if selected else MODES
    for mode in modes:
        folder=output/mode;project=folder/'project';generate(project,'dark')
        field(project/'field.bin',mode)
        environment=(0,0,0,1) if mode in ('snapshot','receiver-snapshot') else (.12,.08,.04,1)
        (project/'global.azenv').write_bytes(b'AZENV001'+struct.pack('<III',2,1,1)+struct.pack('<8e',*(environment*2)))
        level=json.loads((project/'level.azurelevel').read_text())
        level['renderSettings']['skyIntensity']=1
        level['irradianceVolume']=dict(version=1,asset='assets:/field.bin',enabled=mode!='disabled-binding',
                                       intensity=1 if mode=='receiver-grid' else 1/16 if mode in ('receiver-offset','receiver-combined','receiver-control')+FRAME_FIELD else 1/128 if mode=='half-intensity' else 1/64)
        if mode.startswith('receiver-'):
            level['irradianceVolume'].update(version=2,sampling=dict(
                normalOffset=.25 if mode in ('receiver-offset','receiver-combined','receiver-grid','receiver-snapshot','receiver-npr','receiver-npr-frame')+FRAME_FIELD else 0,
                jitterAmplitude=.2 if mode in ('receiver-jitter','receiver-combined','receiver-grid','receiver-snapshot','receiver-npr','receiver-frame-pixel','receiver-npr-frame')+FRAME_FIELD else 0,phase=49.875))
        if mode in FRAME_BOTTOM+FRAME_FIELD:
            level['irradianceVolume']['version']=3
            level['irradianceVolume']['sampling'].update(pixelOrigin='bottom-left' if mode in FRAME_BOTTOM else 'top-left',
                jitterSpace='field' if mode in FRAME_FIELD or mode=='receiver-npr-frame' else 'world')
        if mode in ('msaa4','snapshot','receiver-snapshot'):level['renderSettings']['schemaVersion']=19
        if mode=='msaa4':level['renderSettings']['antiAliasing']=4
        if mode in ('snapshot','receiver-snapshot'):level['renderSettings']['screenSpaceReflection']=dict(schemaVersion=1,enabled=True,maxSteps=32)
        (project/'level.azurelevel').write_text(json.dumps(level,indent=2))
        if mode in NPR:
            gltf=json.loads((project/'plane.gltf').read_text())
            gltf['materials'][0]['extras']=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=[],
                styleParameters=[1,1,0,0],nprDiffuse=dict(schemaVersion=1,shadowScale=1,
                    lightingInputs='host-scene-radiance',shadowMask='none',baseAo='albedo-alpha',shadowAlbedo='base-color')))
            (project/'plane.gltf').write_text(json.dumps(gltf))
        if mode in ('mirrored','receiver-offset','receiver-combined','receiver-control')+FRAME_FIELD:
            gltf=json.loads((project/'plane.gltf').read_text());prefix='data:application/octet-stream;base64,'
            raw=bytearray(base64.b64decode(gltf['buffers'][0]['uri'][len(prefix):]))
            struct.pack_into('<12f',raw,48,*([1,0,0]*4));gltf['buffers'][0]['uri']=prefix+base64.b64encode(raw).decode()
            (project/'plane.gltf').write_text(json.dumps(gltf))
        command=[str(player.resolve()),'--project',str(project/'lab.azureproject'),'--environment',str(project/'global.azenv'),
            '--width','128','--height','128','--fixed-frame-step','--capture-dir',str(folder/'frames'),'--capture-frames','1',
            '--runtime-report',str(folder/'runtime.json'),'--gpu-timing-output',str(folder/'gpu.json'),
            '--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty' if mode in ('snapshot','receiver-snapshot') else 'ambient']
        if mode=='legacy' or fixed:command.append('--disable-bindless')
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
        gpu=json.loads((folder/'gpu.json').read_text());assert gpu['irradianceResourceKind']==('grid' if mode=='receiver-grid' else 'clipmap'),mode
        assert gpu['irradianceQueryScope']=='physical-surface',mode
        sampling=level['irradianceVolume'].get('sampling',dict(normalOffset=0,jitterAmplitude=0,phase=0))
        assert all(gpu['irradianceQuery'][key]==value if isinstance(value,str) else abs(gpu['irradianceQuery'][key]-value)<1e-6 for key,value in sampling.items()),mode
        picture=Image.open(folder/'frames/frame_000000.png').convert('RGB');measurements=[]
        for x,y in ((64,64),(40,40),(88,88)):
            if mode in FRAME_BOTTOM:y=127-y
            pixel=picture.getpixel((x,y));linear=[v/255/12.92 if v/255<=.04045 else ((v/255+.055)/1.055)**2.4 for v in pixel]
            truth=expected(mode,(x+.5)/128*8-4,x)
            measurements.append(dict(pixel=list(pixel),linear=linear,expected=truth,error=max(abs(a-b) for a,b in zip(linear,truth))))
        results[mode]=dict(measurements=measurements,resourceKind=gpu['irradianceResourceKind'],
                           validationErrors=0,resourcesReleased=True)
        if mode in ('receiver-npr','receiver-npr-frame'):
            control=Image.open(output/'receiver-npr-control/frames/frame_000000.png').convert('RGB')
            assert picture.tobytes()==control.tobytes(),'NPR receivers must preserve their surface query'
            results[mode]['controlImageExact']=True
        if mode in ('receiver-offset','receiver-combined','receiver-jitter','receiver-grid'):
            controls=[]
            for x in (64,40,88):
                world_x=(x+.5)/128*8-4
                control=([.05+.5*(world_x+4)/8+.5,.05+.05*(-world_x+4)/8,.125]
                         if mode=='receiver-grid' else expected('baseline' if mode=='receiver-jitter' else 'receiver-control',world_x,x))
                controls.append(max(abs(a-b) for a,b in zip(expected(mode,world_x,x),control)))
            results[mode]['omittedQueryMaximumGap']=max(controls)
            assert max(controls)>.024,(mode,'Query effect must exceed twice the pixel budget',controls)
        (output/'summary.json').write_text(json.dumps(results,indent=2))
        assert max(m['error'] for m in measurements)<.012,(mode,measurements)
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--case',choices=MODES)
    parser.add_argument('--fixed',action='store_true')
    args=parser.parse_args()
    if args.output:results=run(args.player,args.output.resolve(),args.case,args.fixed)
    else:
        with tempfile.TemporaryDirectory(prefix='azure-clipmap-scene-') as temporary:
            results=run(args.player,Path(temporary)/'result',args.case,args.fixed)
    print(json.dumps({k:max(m['error'] for m in v['measurements']) for k,v in results.items()}))
