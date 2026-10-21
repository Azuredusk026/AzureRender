"""Validate world-anchored light influence boxes with actual Player frames."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

from light_lab import generate


def run(player, output, tracing=False):
    import numpy as np
    from PIL import Image
    player_sha=hashlib.sha256(player.read_bytes()).hexdigest()
    pictures={};cases={}
    # Unit coordinates: [-1,1]. Rotated case turns its narrow axis 90 degrees.
    rows={
        'clear':None,
        'axis':[[.5,0,0,0],[0,1,0,0],[0,0,1,0]],
        'rotated':[[0,.5,0,0],[-1,0,0,0],[0,0,1,0]],
        'translated':[[.5,0,0,-.5],[0,1,0,0],[0,0,1,0]],
    }
    for name,box in rows.items():
        folder=output/name;generate(folder,'directional-near')
        level=folder/'level.azurelevel';data=json.loads(level.read_text())
        if box is not None:
            data['lights'][0]['shape']['influenceBox']={'enabled':True,'worldToBox':box,'falloffThreshold':.5}
        if tracing:data['renderSettings'].update(renderer='pathtracing',pathTraceSamples=64,pathTraceBounces=2,pathTraceSeed=7)
        level.write_text(json.dumps(data,indent=2))
        command=[str(player),'--project',str(folder/'lab.azureproject'),'--width','256','--height','256',
                 '--runtime-report',str(folder/'runtime.json')]
        if tracing:
            steps={'schemaVersion':1,'steps':[{'op':'wait-frames','frames':32,'timeoutMs':60000},
                {'op':'wait-frames','frames':32,'timeoutMs':60000},{'op':'wait-frames','frames':5},{'op':'screenshot','label':'final'}]}
            (folder/'steps.json').write_text(json.dumps(steps))
            command+=['--resource-root',str(folder),'--validation-script',str(folder/'steps.json'),
                      '--validation-report',str(folder/'validation.json'),'--smoke-frames','100']
            frame=folder/'captures/final.png'
        else:
            command+=['--capture-dir',str(folder/'frames'),'--capture-frames','1','--capture-fps','30','--fixed-frame-step']
            frame=folder/'frames/frame_000000.png'
        (folder/'command.json').write_text(json.dumps(command,indent=2))
        with (folder/'run.log').open('w') as log:
            proc=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=120)
        log=(folder/'run.log').read_text();runtime=json.loads((folder/'runtime.json').read_text())
        assert proc.returncode==0 and not re.search(r'VUID-|Validation Error|\[error\]|\[validation error\]',log,re.I),name
        assert 'Allocator after unload: buffers=0 images=0' in log,name
        assert not runtime['scriptErrors'] and not runtime['presentationErrors'],name
        if tracing:
            assert json.loads((folder/'validation.json').read_text())['passed']
            assert runtime['renderer']=='pathtracing' and runtime['renderProgress']['samples']==64
        pixels=np.asarray(Image.open(frame).convert('RGB'),dtype=float)/255
        pictures[name]=np.where(pixels<=.04045,pixels/12.92,((pixels+.055)/1.055)**2.4)
        cases[name]={'levelSha256':hashlib.sha256(level.read_bytes()).hexdigest(),
                     'frameSha256':hashlib.sha256(frame.read_bytes()).hexdigest(),'released':True}
    # Hand selected full-bright, transition and outside samples on both axes.
    # Ortho mapping is 8 metres over 256 pixels, sampled at pixel centres.
    errors=[]
    for name,box in rows.items():
        if box is None:continue
        for px,py in [(128,128),(184,128),(200,128),(128,100),(128,84),(216,128)]:
            point=[(px+.5)/32-4,4-(py+.5)/32,0,1]
            edge=max(abs(sum(a*b for a,b in zip(row,point))) for row in box)
            expected=(1-min(1,max(0,(edge-.75)/.25)))**2
            reference=float(pictures['clear'][py,px].mean());assert reference>.01
            actual=float(pictures[name][py,px].mean())/reference
            errors.append(abs(actual-expected))
    result={'playerSha256':player_sha,'renderer':'pathtracing' if tracing else 'raster',
            'maximumAbsoluteWeightError':max(errors),'errorMaximum':.02,'cases':cases}
    assert hashlib.sha256(player.read_bytes()).hexdigest()==player_sha
    (output/'metrics.json').write_text(json.dumps(result,indent=2))
    assert max(errors)<.02,result
    print(json.dumps(result))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--player',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
    p.add_argument('--tracing',action='store_true');a=p.parse_args()
    run(a.player.resolve(),a.output.resolve(),a.tracing)
