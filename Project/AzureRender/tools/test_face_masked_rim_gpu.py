"""Verify face-mask rim response using independent head and light geometry."""
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
from native_render.light_lab import generate
from test_npr_diffuse_gpu import linear
from create_visibility_fixture import pad_material_profile_index


def run(player, output, validation, aa):
    output.mkdir(parents=True,exist_ok=False)
    inputs=[player,Path(__file__),Path(__file__).with_name('test_npr_diffuse_gpu.py'),
        Path(__file__).parent/'native_render/light_lab.py',*sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprint=lambda:{str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    frozen=fingerprint();records=[];images={}
    cases=[dict(name='reconstructed-normal',mask=255,reconstructed=True),dict(name='mask-zero',mask=0),dict(name='mask-half',mask=128),dict(name='mask-one',mask=255),
        dict(name='narrow-edge',mask=0,width=.2,edge=True),dict(name='mixed-edge',mask=0,width=.35,edge=True),
        dict(name='turned',mask=255,angle=45),dict(name='transition',mask=255,angle=20),
        dict(name='back-aligned',mask=255,angle=180),dict(name='near-depth',mask=255,depth=-.05),
        dict(name='far-depth',mask=255,depth=-.3),dict(name='no-receiver',mask=255,receiver=False),
        dict(name='disabled',mask=255,enabled=False),dict(name='effect-disabled',mask=255,effect=False)]
    for fixed in [False,True]:
        backend='fixed' if fixed else 'indexed'
        for case in cases:
            name=backend+'-'+case['name'];folder=output/name;generate(folder,'dark')
            model=json.loads((folder/'plane.gltf').read_text());material=model['materials'][0]
            normal_pixel=[255,128,140] if case.get('edge') else [204,128,230]
            for filename,color,size in [('sdf',(128,128,128,255),(2,2)),('mask',(0,0 if case.get('reconstructed') else 255,0,case['mask']),(2,2)),
                ('ramp',(255,255,255,255),(2,1)),('normal',(*normal_pixel,255),(1,1))]:
                Image.new('RGBA',size,color).save(folder/(filename+'.png'))
            model.update(images=[dict(uri=n+'.png') for n in ['sdf','mask','ramp','normal']],
                textures=[dict(source=i) for i in range(4)])
            angle=math.radians(case.get('angle',0));head=len(model['nodes'])
            model['nodes'].append(dict(name='RimFrame',rotation=[0,math.sin(angle/2),0,math.cos(angle/2)]))
            model['scenes'][0]['nodes'].append(head)
            material['pbrMetallicRoughness'].update(baseColorFactor=[.25,.25,.25,1],metallicFactor=0,roughnessFactor=1)
            material['normalTexture']=dict(index=3)
            material['extras']=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'face'},features=['face-sdf-eligible'],
                styleParameters=[1,1,0,0],faceSdf=dict(schemaVersion=2,texture=0,maskTexture=1,rampTexture=2,texCoord=0,
                    headNode='RimFrame',headBasis=[1,0,0,0,1,0,0,0,1]),
                nprDiffuse=dict(schemaVersion=1,shadowScale=1,lightingInputs='host-scene-radiance',shadowMask='none',
                    baseAo='one',shadowAlbedo='base-color',rimResponse='face-masked')))
            level=json.loads((folder/'level.azurelevel').read_text())
            level['nodes'][0]['components']={'azure.transform':dict(type='azure.transform',version=1,data=dict(scale=[.35]*3))}
            position=[2,0,1];level['nodes'][2]['components']['azure.transform']['data']['translation']=position
            width=case.get('width',.4)
            level['lights']=[dict(id='rim',nodeId='light',enabled=case.get('enabled',True),color=[.8,.6,.4],intensity=1.5,radius=10,
                shape=dict(type='point',falloff='legacy',receivers=dict(schemaVersion=1,surfaces=True,characters=case.get('receiver',True)),
                    characterResponse=dict(schemaVersion=5,composition='material-rim',width=width,surfaceTint=.8)))]
            level['renderSettings']['nprLighting']=dict(schemaVersion=1,enabled=True,ambientColor=[0,0,0],ambientDirection=[0,1,0],
                directionBias=0,directionGain=0,directionFloor=1,outdoorGain=1,shadowAmbientGain=1,indoorMix=0,
                litAmbientMix=0,mainColorMix=0,shadowOverride=1,useSceneIrradiance=False)
            if 'depth' in case:
                back=copy.deepcopy(model);back['materials'][0].pop('extras');back['materials'][0]['pbrMetallicRoughness']['baseColorFactor']=[0,0,0,1]
                (folder/'back.gltf').write_text(json.dumps(back))
                level['resources'].append(dict(id='back',asset='assets:/back.gltf'))
                level['nodes'].append(dict(id='back',resourceId='back',components={'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[0,0,case['depth']]))}))
            pad_material_profile_index(model);(folder/'plane.gltf').write_text(json.dumps(model));(folder/'level.azurelevel').write_text(json.dumps(level))
            command=[str(player),'--project',str(folder/'lab.azureproject'),'--width','128','--height','128','--capture-dir',str(folder/'frames'),
                '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--set','render.antiAliasing='+str(aa)]
            if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            if case.get('effect') is False:command+=['--qa-effect','rim','--qa-effect-state','disabled']
            startup=None
            if hasattr(subprocess,'STARTUPINFO'):
                startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
            result=subprocess.run(command,capture_output=True,startupinfo=startup,timeout=180)
            log=result.stdout+result.stderr;(folder/'player.log').write_bytes(log);text=log.decode(errors='replace')
            assert result.returncode==0 and 'Allocator after unload: buffers=0 images=0' in text,(name,text[-1500:])
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text),name
            assert f'Character texture access: {backend}' in text,name
            if validation!='any':assert ('Validation layer: enabled' in text)==(validation=='enabled'),name
            image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'));images[backend,case['name']]=image
            samples=[(.375,.125),(.875,.375),(.125,.625),(.625,.875)] if aa==4 else [(.5,.5)]
            expected=[]
            normal=np.asarray(normal_pixel)/255*2-1;normal/=np.linalg.norm(normal)
            head_weight=np.clip((abs(math.cos(angle))-.9)*10,0,1);head_weight=head_weight**2*(3-2*head_weight)
            for sx,sy in samples:
                point=np.array([(84+sx)/128*8-4,4-(64+sy)/128*8,0]);delta=np.asarray(position)-point;squared=np.dot(delta,delta)
                light=delta/np.sqrt(squared);planar=light.copy();planar[2]=0;planar/=np.linalg.norm(planar)
                view=np.array([0,0,5])-point;view/=np.linalg.norm(view)
                band=np.clip(((1-abs(np.dot(normal,view)))-(.8-.6*width))/(.1+.2*width),0,1);band=band**2*(3-2*band)
                weight=head_weight*((1-np.clip(width*10-3,0,1))*band+np.clip(width*10-3,0,1)*case['mask']/255)
                attenuation=(1-np.sqrt(squared)/10)**2/(1+.12*squared)
                color=np.asarray([.8,.6,.4])*1.5*attenuation*weight*np.clip(np.dot(normal,planar),0,1)*(.5+.8*(.25*.96-.5))
                if case.get('reconstructed'):color[:]=0  # Neutral blue bounds the planar response below .002 for either mirrored side.
                if case.get('receiver') is False or case.get('enabled') is False or case.get('effect') is False:color[:]=0
                expected.append(color)
            expected=np.mean(expected,axis=0);error=float(abs(linear(image[64,84])-expected).max());assert error<=.008,(name,error,linear(image[64,84]).tolist(),expected.tolist())
            if fixed:assert np.array_equal(image,images['indexed',case['name']]),name
            records.append(dict(name=name,maximumLinearError=error,expected=expected.tolist(),command=command));print(name,'passed',flush=True)
        assert np.array_equal(images[backend,'near-depth'][64,84],images[backend,'far-depth'][64,84])
    assert fingerprint()==frozen
    (output/'report.json').write_text(json.dumps(dict(status='passed',frames=len(cases)*2,antiAliasing=aa,descriptorExact=True,cases=records,inputFingerprints=frozen),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path)
    p.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');p.add_argument('--anti-aliasing',type=int,choices=[0,4],default=0);a=p.parse_args()
    if a.output:run(a.executable.resolve(),a.output.resolve(),a.expected_validation,a.anti_aliasing)
    else:
        with tempfile.TemporaryDirectory(prefix='face-rim-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(a.executable.resolve(),Path(temporary)/'check',a.expected_validation,a.anti_aliasing)
