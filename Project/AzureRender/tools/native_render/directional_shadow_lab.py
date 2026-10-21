"""Measure scene-directional shadows through actual Player frames."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from light_lab import generate


def run(player, root):
    from PIL import Image
    import numpy as np
    pictures={}
    cases={}
    player_sha=hashlib.sha256(player.read_bytes()).hexdigest()
    for name,blocked in [('clear',False),('blocked',True)]:
        folder=root/name
        generate(folder,'directional-near')
        level=folder/'level.azurelevel';data=json.loads(level.read_text())
        data['lights'][0]['shape']['direction']=[.6,0,-.8]
        data['renderSettings'].update(shadowRadius=1,shadowDistance=20)
        if blocked:
            data['nodes'].append({'id':'blocker','resourceId':'plane','components':{
                'azure.transform':{'type':'azure.transform','version':1,'data':{
                    'translation':[0,0,1],'scale':[.125,.125,1]}}}})
        level.write_text(json.dumps(data,indent=2))
        command=[str(player),'--project',str(folder/'lab.azureproject'),'--width','256','--height','256',
                 '--capture-dir',str(folder/'frames'),'--capture-frames','1','--capture-fps','30',
                 '--fixed-frame-step','--runtime-report',str(folder/'runtime.json')]
        (folder/'command.json').write_text(json.dumps(command,indent=2))
        with (folder/'run.log').open('w') as log:
            proc=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=120)
        log=(folder/'run.log').read_text();runtime=json.loads((folder/'runtime.json').read_text())
        assert proc.returncode==0 and not re.search(r'VUID-|Validation Error|\[error\]|\[validation error\]',log,re.I)
        assert 'Allocator after unload: buffers=0 images=0' in log
        assert not runtime['scriptErrors'] and not runtime['presentationErrors']
        image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'),dtype=float)/255
        pictures[name]=np.where(image<=.04045,image/12.92,((image+.055)/1.055)**2.4)
        cases[name]={'levelSha256':hashlib.sha256(level.read_bytes()).hexdigest(),
                     'frameSha256':hashlib.sha256((folder/'frames/frame_000000.png').read_bytes()).hexdigest()}
    # An elevated blocker projects to x=+.75 under this emitted direction.
    shadow=(slice(124,132),slice(148,156))
    control=(slice(124,132),slice(88,96))
    clear=float(pictures['clear'][shadow].mean());blocked=float(pictures['blocked'][shadow].mean())
    ratio=blocked/clear
    control_error=float(abs(pictures['clear'][control]-pictures['blocked'][control]).mean())
    result=dict(clearLinear=clear,blockedLinear=blocked,shadowRatio=ratio,controlLinearError=control_error,
                shadowRatioMaximum=.05,controlErrorMaximum=.005,released=True,playerSha256=player_sha,cases=cases)
    assert hashlib.sha256(player.read_bytes()).hexdigest()==player_sha,'Player changed during capture'
    (root/'metrics.json').write_text(json.dumps(result,indent=2))
    assert clear>.05,result
    assert ratio<.05,result
    assert control_error<.005,result
    print(json.dumps(result))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--player',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args();run(args.player.resolve(),args.output.resolve())
