"""Verify configured reflection LOD through public scene and Player paths."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from test_cube_probe_gpu import fixture,observation_errors,BUDGET

COLORS=np.asarray([[.8,.1,.1],[.1,.8,.1],[.1,.1,.8],[.8,.8,.1],
                  [.1,.8,.8],[.8,.1,.8],[.4,.4,.4],[.2,.2,.2]])
LOG=dict(schemaVersion=1,kind='logarithmic',scale=1.2,bias=5,minimumRoughness=.001)


def cases():
    # GltfLoader quantizes imported roughness to RGBA8. Nonlinear references
    # use D3D11 readbacks at those exact UNORM values; the source power-of-two
    # and captured-pixel references are separately checked by the CPU contract.
    return [dict(name='legacy-linear',roughness=.5,lod=3.5137254901960784),
        dict(name='explicit-linear',roughness=.5,lod=3.5137254901960784,mapping=dict(schemaVersion=1,kind='linear')),
        dict(name='log-one',roughness=1,lod=5,mapping=LOG),
        dict(name='log-half',roughness=.5,lod=3.8067760467529297,mapping=LOG),
        dict(name='log-quarter',roughness=.25,lod=2.606776237487793,mapping=LOG),
        dict(name='minimum-roughness',roughness=.5,lod=4.501955032348633,mapping=dict(LOG,minimumRoughness=.75)),
        dict(name='constant-lod',roughness=.5,lod=1.25,mapping=dict(LOG,scale=0,bias=1.25)),
        dict(name='upper-clamp',roughness=.5,lod=7,mapping=dict(LOG,bias=20)),
        dict(name='lower-clamp',roughness=.5,lod=0,mapping=dict(LOG,bias=-20)),
        dict(name='snapshot',roughness=.5,lod=3.8067760467529297,mapping=LOG,snapshot=True),
        dict(name='msaa4',roughness=.25,lod=2.606776237487793,mapping=LOG,msaa=True)]


def prepare(root,case):
    project=fixture(root,4,case['roughness'])
    path=root/'assets/scene.azurelevel';level=json.loads(path.read_text())
    if 'mapping' in case:level['reflectionProbe'].update(version=3,lodMapping=case['mapping'])
    if case.get('snapshot'):level['renderSettings']['screenSpaceReflection']=dict(schemaVersion=1,enabled=True,maxSteps=8)
    if case.get('msaa'):level['renderSettings'].update(schemaVersion=19,antiAliasing=4)
    path.write_text(json.dumps(level,indent=2))
    payload=bytearray(b'AZENV002'+struct.pack('<III',128,128,8))
    for mip,color in enumerate(COLORS):
        size=max(1,128>>mip);payload.extend(struct.pack('<4e',*color,1)*(size*size*6))
    (root/'assets/probe.azenv').write_bytes(payload)
    lo=math.floor(case['lod']);hi=min(7,lo+1);fraction=case['lod']-lo
    # Use the exact stored half values, independently of GPU mapping math.
    stored=COLORS.astype('<f2').astype(float);wanted=stored[lo]*(1-fraction)+stored[hi]*fraction
    (root/'expected.json').write_text(json.dumps(dict(case=case,sampledRoughness=math.floor(case['roughness']*255+.5)/255,
        linearRgb=wanted.tolist(),budget=BUDGET),indent=2))
    return project,wanted


def run(player,output,selected=None):
    output.mkdir(parents=True,exist_ok=False);report=dict(schemaVersion=1,budget=BUDGET,status='running',captures=[])
    report['playerSha256']=hashlib.sha256(player.read_bytes()).hexdigest()
    chosen=[c for c in cases() if not selected or c['name']==selected]
    for case in chosen:
        for indexed in [True,False]:
            folder=output/(case['name']+('-indexed' if indexed else '-fixed'));project,wanted=prepare(folder/'project',case)
            command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128',
                '--capture-dir',str((folder/'frames').resolve()),'--capture-frames','1','--fixed-frame-step',
                '--qa-camera','scene','--qa-light','scene','--qa-isolation','reflection',
                '--gpu-timing-output',str((folder/'gpu.json').resolve())]
            if not indexed:command.append('--disable-bindless')
            (folder/'command.json').write_text(json.dumps(command,indent=2))
            startup=None
            if hasattr(subprocess,'STARTUPINFO'):
                startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
            with (folder/'player.log').open('w') as log:
                subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180,startupinfo=startup)
            log=(folder/'player.log').read_text(errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log)
            assert 'Allocator after unload: buffers=0 images=0' in log
            image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))
            errors=observation_errors(image[62:66,62:66],wanted)
            assert errors['maximumQuantizationIntervalLinearError']<=BUDGET,(case['name'],errors,wanted)
            if not indexed:
                previous=np.asarray(Image.open(output/(case['name']+'-indexed')/'frames/frame_000000.png').convert('RGB'))
                assert np.array_equal(image,previous),'Descriptor paths differ'
            gpu=json.loads((folder/'gpu.json').read_text())
            if case.get('msaa'):
                manifest=json.loads((folder/'frames/capture_manifest.json').read_text());assert manifest['rasterizationSamples']==4
            row=dict(name=case['name'],indexed=indexed,expectedLod=case['lod'],expectedLinearRgb=wanted.tolist(),
                     **errors,validationErrors=0,resourcesReleased=True)
            report['captures'].append(row);(output/'summary.json').write_text(json.dumps(report,indent=2))
    assert chosen,'Unknown LOD case';report['status']='passed'
    (output/'summary.json').write_text(json.dumps(report,indent=2));return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--player',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--case',choices=[c['name'] for c in cases()]);args=parser.parse_args()
    if args.output:result=run(args.player,args.output.resolve(),args.case)
    else:
        with tempfile.TemporaryDirectory(prefix='azure-reflection-lod-') as temporary:result=run(args.player,Path(temporary)/'result',args.case)
    print(json.dumps(dict(status=result['status'],captures=len(result['captures']),maximumError=max(x['maximumQuantizationIntervalLinearError'] for x in result['captures']))))
