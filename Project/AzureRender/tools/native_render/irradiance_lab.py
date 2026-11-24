"""Compare public directional-grid GPU output with independent CPU interpolation."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import shutil

from native_render.light_lab import generate


def field(path, missing=False, outside=False):
    origin=(20,-4,-4) if outside else (-4,-4,-4)
    cells=[]
    for z in range(2):
        for y in range(2):
            for x in range(2):
                cells.append([[0,0,.4*x,-.1],[0,0,0,.2+.2*y],[0,0,.1*z,.35],
                              [0 if missing and x==y==z==0 else 1,0,0,0]])
    path.write_bytes(struct.pack('<8s4f3I',b'AZIV0001',*origin,8,2,2,2)
                     +b''.join(struct.pack('<16f',*(v for row in cell for v in row)) for cell in cells))
    return cells


def truth(cells, position, intensity=1):
    fractions=[(p+4)/8 for p in position]
    coefficients=[[0]*4 for _ in range(3)]
    weight=0
    for z in range(2):
        for y in range(2):
            for x in range(2):
                c=cells[x+2*y+4*z]
                w=c[3][0]
                for bit,f in zip((x,y,z),fractions):w*=f if bit else 1-f
                weight+=w
                for channel in range(3):
                    for a in range(4):coefficients[channel][a]+=w*c[channel][a]
    return [max(c[2]+c[3],0)*intensity+(1-weight)*e for c,e in zip(coefficients,(.12,.08,.04))]


def run(player, output):
    from PIL import Image
    output.mkdir(parents=True,exist_ok=False)
    project=output/'project';generate(project,'dark')
    (project/'global.azenv').write_bytes(b'AZENV001'+struct.pack('<III',2,1,1)+struct.pack('<8e',.12,.08,.04,1,.12,.08,.04,1))
    base=json.loads((project/'level.azurelevel').read_text());base['renderSettings']['skyIntensity']=1
    results={}
    for name in ('disabled','directional','half-intensity','missing','outside','legacy'):
        cells=field(project/'field.aziv',missing=name=='missing',outside=name=='outside')
        level=json.loads(json.dumps(base))
        level['irradianceVolume']={'version':1,'asset':'assets:/field.aziv',
            'enabled':name!='disabled','intensity':.5 if name=='half-intensity' else 1}
        (project/'level.azurelevel').write_text(json.dumps(level))
        active_project=project
        if name=='legacy':
            active_project=output/'relocated-project'
            shutil.copytree(project,active_project)
        folder=output/name;folder.mkdir()
        command=[str(player.resolve()),'--project',str(active_project/'lab.azureproject'),
            '--environment',str(project/'global.azenv'),'--width','128','--height','128',
            '--capture-dir',str(folder/'frames'),'--capture-frames','1','--capture-fps','30',
            '--fixed-frame-step','--runtime-report',str(folder/'runtime.json'),
            '--qa-camera','scene','--qa-light','scene','--qa-isolation','ambient']
        if name=='legacy':command.append('--disable-bindless')
        startup=None
        if hasattr(subprocess,'STARTUPINFO'):
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        with (folder/'player.log').open('w') as log:
            subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=90,startupinfo=startup)
        log=(folder/'player.log').read_text(errors='replace')
        assert 'VUID-' not in log and 'Validation Error' not in log,name
        assert 'Allocator after unload: buffers=0 images=0' in log,name
        report=json.loads((folder/'runtime.json').read_text())
        assert not report['presentationErrors'] and not report['scriptErrors'],name
        picture=Image.open(folder/'frames/frame_000000.png').convert('RGB')
        measurements=[]
        for x,y in ((64,64),(40,40),(88,88)):
            position=((x+.5)/128*8-4,4-(y+.5)/128*8,0)
            expected=[.12,.08,.04] if name in ('disabled','outside') else truth(cells,position,.5 if name=='half-intensity' else 1)
            pixel=picture.getpixel((x,y))
            linear=[v/255/12.92 if v/255<=.04045 else ((v/255+.055)/1.055)**2.4 for v in pixel]
            measurements.append(dict(pixel=list(pixel),linear=linear,expected=expected,error=max(abs(a-b) for a,b in zip(linear,expected))))
        results[name]=measurements
    (output/'metrics.json').write_text(json.dumps(results,indent=2))
    assert all(m['error']<.012 for row in results.values() for m in row),results
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=run(args.player,args.output.resolve())
    print(json.dumps({k:max(m['error'] for m in v) for k,v in result.items()}))
