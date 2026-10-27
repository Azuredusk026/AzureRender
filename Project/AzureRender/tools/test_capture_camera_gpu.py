"""Verify captured lens metadata and fingerprints against actual game cameras."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np
from PIL import Image
from test_character_probe_gpu import prepare


def run(player,output):
    output.mkdir(parents=True,exist_ok=False);report=dict(schemaVersion=1,status='running',captures=[])
    report['playerSha256']=hashlib.sha256(player.read_bytes()).hexdigest()
    definitions=[dict(name='source-lens',far=10000,fov=57.22092455245762),
        dict(name='far-only',far=15000,fov=57.22092455245762),
        dict(name='fov-only',far=10000,fov=65),
        dict(name='ortho-base',far=10000,fov=57.22092455245762,ortho=True,height=6),
        dict(name='ortho-wide',far=10000,fov=57.22092455245762,ortho=True,height=8),
        dict(name='roll-only',far=10000,fov=57.22092455245762,roll=15),
        dict(name='one-ulp-position',far=10000,fov=57.22092455245762,cameraZ=3.000000238418579)]
    for renderer in ['character','pathtracing']:
        root=output/(renderer+'-project');project=prepare(root,dict(name='capture-camera',material='pbr',probe=False))
        material_file=root/'assets/surface.gltf';model=json.loads(material_file.read_text())
        model['materials'][0].update(emissiveFactor=[.2,.04,.01])
        model['materials'][0]['pbrMetallicRoughness'].update(baseColorFactor=[0,0,0,1],metallicFactor=0)
        material_file.write_text(json.dumps(model));base=json.loads((root/'assets/scene.azurelevel').read_text())
        base['renderSettings'].update(renderer=renderer,pathTraceSamples=8,pathTraceBounces=1,pathTraceDenoise=False)
        images={};records={}
        for case in definitions:
            scene=copy.deepcopy(base);camera=scene['nodes'][1]['components']['azure.camera'];camera['version']=2
            camera['data'].update(nearPlane=.25,farPlane=case['far'],fieldOfView=case['fov'],
                orthographic=case.get('ortho',False),orthographicHeight=case.get('height',6),projectionOffsetX=0,projectionOffsetY=0)
            scene['nodes'][1]['components']['azure.transform']['data']['rotation']=[0,0,case.get('roll',0)]
            scene['nodes'][1]['components']['azure.transform']['data']['translation']=[0,0,case.get('cameraZ',3)]
            (root/'assets/scene.azurelevel').write_text(json.dumps(scene,indent=2))
            folder=output/(renderer+'-'+case['name']);folder.mkdir()
            command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128',
                '--capture-dir',str((folder/'frames').resolve()),'--capture-frames','1','--fixed-frame-step',
                '--qa-camera','scene','--qa-light','scene']
            (folder/'command.json').write_text(json.dumps(command,indent=2))
            startup=None
            if hasattr(subprocess,'STARTUPINFO'):
                startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
            with (folder/'player.log').open('w') as log:
                subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180,startupinfo=startup)
            log=(folder/'player.log').read_text(errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log)
            assert 'Allocator after unload: buffers=0 images=0' in log
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text())
            assert manifest['cameraNear']==.25 and manifest['cameraFar']==case['far'],(renderer,case['name'],manifest['cameraNear'],manifest['cameraFar'])
            lens=manifest['cameraLens'];assert lens['schemaVersion']==1 and lens['nearPlane']==.25 and lens['farPlane']==case['far']
            assert lens['offsetNdc']==[0,0]
            assert lens['projection']==('orthographic' if case.get('ortho') else 'perspective')
            if case.get('ortho'):assert lens['orthographicHeight']==case['height']
            else:assert abs(lens['verticalFovRadians']-math.radians(case['fov']))<1e-6
            assert manifest['qaStateFingerprintVersion']==2
            angle=math.radians(case.get('roll',0));expected_up=[math.sin(angle),math.cos(angle),0]
            assert max(abs(a-b) for a,b in zip(manifest['cameraUp'],expected_up))<1e-6
            assert manifest['cameraProjection']==dict(schemaVersion=2,offsetNdc=[0,0])
            image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'));images[case['name']]=image
            mask=(image[:,:,0]>20)&(image[:,:,0].astype(float)>image[:,:,1]*1.8)
            area=int(np.count_nonzero(mask));assert area>100,(renderer,case['name'],area)
            row=dict(renderer=renderer,name=case['name'],lens=lens,stateHash=manifest['qaStateHash'],area=area,
                imageSha256=hashlib.sha256(image.tobytes()).hexdigest(),validationErrors=0,resourcesReleased=True)
            report['captures'].append(row);records[case['name']]=row
            (output/'summary.json').write_text(json.dumps(report,indent=2))
        assert records['source-lens']['stateHash']!=records['far-only']['stateHash'],'Far plane must enter the fingerprint'
        assert records['source-lens']['stateHash']!=records['fov-only']['stateHash'],'FOV must enter the fingerprint'
        assert records['ortho-base']['stateHash']!=records['ortho-wide']['stateHash'],'Orthographic height must enter the fingerprint'
        assert records['source-lens']['stateHash']!=records['roll-only']['stateHash'],'Camera roll must enter the fingerprint'
        assert records['source-lens']['stateHash']!=records['one-ulp-position']['stateHash'],'A one-ULP position change must enter the fingerprint'
        assert not np.array_equal(images['source-lens'],images['roll-only']),'Roll must affect the image'
        assert np.array_equal(images['source-lens'],images['far-only']),'Clipping beyond visible geometry must preserve this fixture'
        assert records['fov-only']['area']<records['source-lens']['area']
        assert records['ortho-wide']['area']<records['ortho-base']['area']
    report['status']='passed';(output/'summary.json').write_text(json.dumps(report,indent=2));return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--player',type=Path,required=True)
    parser.add_argument('--output',type=Path);args=parser.parse_args()
    if args.output:result=run(args.player,args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='azure-capture-camera-') as temporary:result=run(args.player,Path(temporary)/'result')
    print(json.dumps(dict(status=result['status'],captures=len(result['captures']))))
