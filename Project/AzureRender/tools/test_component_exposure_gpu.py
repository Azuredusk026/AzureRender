"""Check explicit component exposure through the production Player transfer."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
from PIL import Image
from native_render.light_lab import generate


def linear(value):
    return np.where(value<=.04045,value/12.92,((value+.055)/1.055)**2.4)


def display(value):
    return np.clip(np.where(value<=.0031308,value*12.92,1.055*np.maximum(value,0)**(1/2.4)-.055),0,1)*255


def run(player,output,validation):
    output.mkdir(parents=True,exist_ok=False)
    digest=hashlib.sha256(player.read_bytes()).hexdigest()
    report=dict(schemaVersion=1,status='running',playerSha256=digest,captures=[],comparisons=[])
    try:
        for isolation in ('direct-diffuse','emissive'):
            images={}
            for label,ev in (('default',None),('zero',0),('negative',-2),('positive',2)):
                folder=output/f'{isolation}-{label}';generate(folder,'point')
                level=json.loads((folder/'level.azurelevel').read_text());level['lights'][0]['intensity']=.25
                (folder/'level.azurelevel').write_text(json.dumps(level))
                asset=json.loads((folder/'plane.gltf').read_text());asset['materials'][0]['emissiveFactor']=[.05,.1,.2]
                (folder/'plane.gltf').write_text(json.dumps(asset))
                command=[str(player),'--project',str(folder/'lab.azureproject'),'--width','128','--height','128',
                    '--capture-dir',str(folder/'frames'),'--capture-frames','1','--fixed-frame-step',
                    '--qa-camera','scene','--qa-light','scene','--qa-isolation',isolation,'--set','render.antiAliasing=0']
                if ev is not None:command+=['--qa-exposure',str(ev)]
                (folder/'command.json').write_text(json.dumps(command,indent=2))
                with (folder/'player.log').open('w',encoding='utf-8') as log:
                    subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
                log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),isolation
                assert 'Allocator after unload: buffers=0 images=0' in log,isolation
                assert ('Validation layer: enabled' in log)==(validation=='enabled'),label
                manifest=json.loads((folder/'frames/capture_manifest.json').read_text(encoding='utf-8-sig'))
                expected=ev if ev is not None else 0
                assert manifest['qaExposureEv']==expected and manifest['gradeSettings']['exposureEv']==expected,manifest['gradeSettings']
                image=folder/'frames/frame_000000.png';images[label]=np.asarray(Image.open(image).convert('RGBA'))
                report['captures'].append(dict(isolation=isolation,label=label,exposureEv=expected,
                    imageSha256=hashlib.sha256(image.read_bytes()).hexdigest(),validationEnabled=validation=='enabled'))
            assert np.array_equal(images['default'],images['zero']),isolation
            baseline=images['zero'][:,:,:3].astype(float)
            assert baseline.max()<245 and baseline.max()>20,('Reference must be visible and unsaturated',isolation)
            for label,ev in (('negative',-2),('positive',2)):
                actual=images[label][:,:,:3].astype(float)
                # The independent PNG reference denotes a half-code interval.
                # Apply linear exposure to that interval, then re-encode it.
                lower=display(linear(np.maximum(baseline-.5,0)/255)*2**ev)
                upper=display(linear(np.minimum(baseline+.5,255)/255)*2**ev)
                outside=np.maximum(np.maximum(lower-actual,actual-upper),0)
                assert outside.max()<=1,(isolation,label,float(outside.max()))
                assert np.array_equal(images[label][:,:,3],images['zero'][:,:,3]),isolation
                assert not np.array_equal(images[label],images['zero']),('Exposure must affect actual pixels',isolation,label)
                report['comparisons'].append(dict(isolation=isolation,exposureEv=ev,maximumIntervalErrorBytes=float(outside.max())))
        assert hashlib.sha256(player.read_bytes()).hexdigest()==digest
        report['status']='passed'
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:(output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status='passed',captures=len(report['captures']),comparisons=report['comparisons'])))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--validation',choices=('enabled','disabled'),required=True)
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.validation)
    else:
        with tempfile.TemporaryDirectory(prefix='azure-component-exposure-') as temporary:
            run(args.executable.resolve(),Path(temporary)/'captures',args.validation)
