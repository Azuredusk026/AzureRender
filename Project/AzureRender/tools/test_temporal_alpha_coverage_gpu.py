"""Measure animated alpha coverage against independent fourfold rasterization."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image
import test_material_temporal_coverage_gpu as motion
from temporal_coverage_metrics import correlated_detail


def fixture(folder,case,mode):
    project=motion.fixture(folder,False,mode)
    path=folder/'assets/panels.gltf';asset=json.loads(path.read_text())
    y,x=np.mgrid[:512,:512]
    # Oblique, resolvable strands with both subpixel and wider coverage.
    phase=(x+y*.19)%32
    alpha=np.where(phase<9,255,0).astype('uint8')
    pixels=np.full((512,512,4),255,dtype='uint8');pixels[:,:,3]=alpha
    image=len(asset['images']);asset['images'].append({'uri':motion.hair.image_uri(Image.fromarray(pixels,'RGBA'))})
    texture=len(asset['textures']);asset['textures'].append({'source':image})
    for material in asset['materials']:
        material.pop('extras',None)
        material['pbrMetallicRoughness'].update(baseColorTexture={'index':texture},baseColorFactor=[.35,.55,.7,1])
        material['alphaMode']={'mask':'MASK','blend':'BLEND','fading':'BLEND','fade-cycle':'BLEND','cloth':'OPAQUE','composite':'BLEND'}[case]
        material['alphaCutoff']=.5
        if case=='composite':material['extras']={'azureRenderMaterial':{'schemaVersion':2,
            'class':'generic' if folder.name=='candidate' else 'overlay','features':['multiply-composite'],
            'composite':{'schemaVersion':1,'mode':'multiply'}}}
    if case in ('fading','fade-cycle'):
        asset.setdefault('extensionsUsed',[]).append('AZURE_node_opacity')
        asset['nodes'][0].setdefault('extensions',{})['AZURE_node_opacity']={'schemaVersion':1,'opacity':.5}
    if case=='fade-cycle':
        binary=bytearray(base64.b64decode(asset['buffers'][0]['uri'].split(',',1)[1]))
        def scalar(values):
            while len(binary)%4:binary.append(0)
            view=len(asset['bufferViews']);asset['bufferViews'].append({'buffer':0,'byteOffset':len(binary),'byteLength':len(values)*4})
            binary.extend(struct.pack('<'+'f'*len(values),*values));index=len(asset['accessors'])
            asset['accessors'].append({'bufferView':view,'componentType':5126,'count':len(values),'type':'SCALAR'});return index
        animation=asset['animations'][0];sampler=len(animation['samplers'])
        animation['samplers'].append({'input':scalar([0,16/60,24/60,40/60,48/60,80/60]),
            'output':scalar([.5,.5,0,0,.5,.5]),'interpolation':'LINEAR'})
        animation['extensions']={'AZURE_node_opacity':{'schemaVersion':1,'channels':[{'node':0,'sampler':sampler}]}}
        asset['buffers'][0].update(byteLength=len(binary),uri='data:application/octet-stream;base64,'+base64.b64encode(binary).decode())
    path.write_text(json.dumps(asset))
    return project


def coherent_detail(candidate,reference,indices=slice(16,48)):
    """Project onto the reference signal so uncorrelated noise cannot mimic detail."""
    return correlated_detail(motion.local(candidate),motion.local(reference),indices)


def run(player,output,case,validation):
    output.mkdir(parents=True,exist_ok=False)
    fingerprints={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (player.parent/'shaders').glob('*.spv')}
    report=dict(schemaVersion=1,status='running',case=case,playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(),shaders=fingerprints,captures=[])
    try:
        images={}
        plans=[('source',1,3),('candidate',1,3)] if case=='composite' else [('source',1,0),('candidate',1,3),('reference',4,0)]
        for name,scale,mode in plans:
            folder=output/name;project=fixture(folder,case,mode)
            command=[str(player),'--project',str(project),'--width',str(128*scale),'--height',str(128*scale),
                '--capture-dir',str(folder/'frames'),'--capture-frames',str(motion.FRAMES),'--capture-fps',str(motion.FPS),
                '--fixed-frame-step','--qa-animation','--qa-camera','scene','--qa-light','scene','--qa-isolation','albedo']
            (folder/'command.json').write_text(json.dumps(command,indent=2))
            with (folder/'player.log').open('w',encoding='utf-8') as log:
                subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log)
            assert 'Allocator after unload: buffers=0 images=0' in log
            assert ('Validation layer: enabled' in log)==(validation=='enabled')
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text(encoding='utf-8-sig'))
            assert manifest['antiAliasing']==mode and manifest['qaAnimation']
            images[name]=motion.frames(folder,scale)
            report['captures'].append(dict(name=name,manifest=manifest,images={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((folder/'frames').glob('frame_*.png'))}))
        if case=='composite':
            source=motion.encoded(images['source']);candidate=motion.encoded(images['candidate'])
            q=dict(maximumCompatibilityByteError=float(np.abs(source-candidate).max()),
                maximumVisibleContrastBytes=float(np.abs(source[:,34:94,34:94]-source[:,0:1,0:1]).max()))
            report['quality']=q
            assert q['maximumVisibleContrastBytes']>5,q
            assert q['maximumCompatibilityByteError']<=1,q
        else:
            q=motion.measure(images['source'],images['candidate'],images['reference']);report['quality']=q
            if case!='cloth':
                active=np.r_[16:24,41:48] if case=='fade-cycle' else slice(16,48)
                q['coherentDetailRatios']=coherent_detail(images['candidate'],images['reference'],active)
            if case=='fade-cycle':
                q['maximumHiddenByteError']=float(np.max(np.abs(motion.encoded(images['candidate'][25:40])-motion.encoded(images['reference'][25:40,0:1,0:1]))))
                assert q['maximumHiddenByteError']<=1,q
            assert all(abs(v-1)<=.02 for v in q['movingEnergyRatios']+q['lastEightEnergyRatios']),q
            assert q['errorRatio']<=.8 and q['residualChangeRatio']<=.8 and q['edgeErrorRatio']<=.8,q
            assert q['maximumGhostPixelsOutsideTwoPixels']==0 and q['maximumFlatByteError']<=1,q
            if case!='cloth':assert all(.8<=v<=1.2 for v in q['coherentDetailRatios']),q
        assert fingerprints=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (player.parent/'shaders').glob('*.spv')}
        assert report['playerSha256']==hashlib.sha256(player.read_bytes()).hexdigest()
        report['status']='passed'
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:
        (output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(q))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--case',choices=('mask','blend','fading','fade-cycle','cloth','composite'),required=True)
    parser.add_argument('--validation',choices=('enabled','disabled'),required=True);args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.case,args.validation)
    else:
        with tempfile.TemporaryDirectory(prefix='alpha-coverage-') as temporary:
            run(args.executable.resolve(),Path(temporary)/'captures',args.case,args.validation)
