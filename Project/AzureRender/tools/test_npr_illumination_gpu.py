"""Measure scene-owned NPR illumination independently of material color."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image
from native_render.light_lab import generate
from create_visibility_fixture import surface,component
from test_npr_diffuse_gpu import reference,linear


def illumination(settings,normal,indirect,main_color,intensity,ao,lit_mask,visibility):
    ao*=settings.get('ambientWeight',1)
    direction=settings['ambientDirection']
    factor=np.clip(np.dot(normal,direction)+settings['directionBias'],0,1)*settings['directionGain']+settings['directionFloor']
    mixed=factor*(np.asarray(indirect)*(1-lit_mask*settings['litAmbientMix'])+lit_mask*settings['litAmbientMix'])
    shadow=mixed*((1-settings['indoorMix'])*min(.65+.35*ao,1.5)+settings['indoorMix']*np.clip(ao,1.25,1.75))*settings['shadowAmbientGain']
    ambient=mixed*np.clip(ao,0,1.5)*(1-settings['mainColorMix']+np.asarray(main_color)*settings['mainColorMix'])*settings['outdoorGain']
    direct=np.asarray(main_color)*intensity
    direct=(np.dot(direct,[.21267290413379669,.7151522040367126,.07217500358819962])*(1-lit_mask)+direct*lit_mask)*settings['outdoorGain']
    return shadow*(1-visibility)+ambient*visibility,direct*visibility


def run(player,output,require_validation=False,selected=None,expected_validation=None):
    output.mkdir(parents=True,exist_ok=False)
    default=dict(schemaVersion=1,enabled=True,ambientColor=[.3,.2,.1],ambientDirection=[0,1,0],
        directionBias=.15,directionGain=1.5,directionFloor=.5,outdoorGain=1.05,shadowAmbientGain=.8,
        indoorMix=0,litAmbientMix=1,mainColorMix=1,shadowOverride=0,useSceneIrradiance=False)
    names=('composed','fixed','direct','ambient','disabled','direction-positive','direction-negative','scene-volume','zero-radiance','point-only','surface-only','additional-directional','main-shadow','main-shadow-floor','indoor-shadow','ambient-weight-quarter','ambient-weight-zero','ambient-weight-direct','ambient-weight-shadow')
    if selected:
        assert set(selected)<=set(names);names=tuple(name for name in names if name in selected)
    summary={'schemaVersion':1,'status':'running','playerSha256':hashlib.sha256(player.read_bytes()).hexdigest(),
        'linearErrorBudget':.008,'captures':[]};frames={};state_hashes={}
    try:
        for name in names:
            print(name,flush=True);folder=output/name;generate(folder,'point' if name=='point-only' else 'directional-near')
            settings=copy.deepcopy(default)
            if name=='disabled':settings['enabled']=False
            if name.startswith('ambient-weight-'):settings['ambientWeight']=0 if name.endswith('zero') else .25
            if name in ('direct','zero-radiance','ambient-weight-direct'):settings.update(ambientColor=[0,0,0],litAmbientMix=0)
            if name in ('direction-positive','direction-negative'):settings['ambientDirection']=[0,0,1 if name.endswith('positive') else -1]
            if name=='scene-volume':settings.update(useSceneIrradiance=True,litAmbientMix=0)
            model_path=folder/'plane.gltf';model=json.loads(model_path.read_text())
            Image.new('RGBA',(1,1),(90,80,70,204)).save(folder/'base.png')
            Image.new('RGBA',(4,2),(60,50,40,255)).save(folder/'lut.png')
            Image.new('RGBA',(2,1),(180,150,170,153)).save(folder/'ramp.png')
            model.update(images=[{'uri':p+'.png'} for p in ('base','lut','ramp')],textures=[{'source':i} for i in range(3)])
            material=model['materials'][0];material['pbrMetallicRoughness'].update(baseColorTexture={'index':0},baseColorFactor=[1,1,1,1])
            material.setdefault('extras',{})['azureRenderMaterial']={'schemaVersion':2,'class':'skin','features':['diffuse-ramp'],
                'shadowColorLut':{'schemaVersion':1,'texture':1,'flipV':False},'diffuseRamp':{'schemaVersion':1,'texture':2},
                'styleParameters':[1,1,0,0],'nprDiffuse':{'schemaVersion':1,'shadowScale':.55,'lightingInputs':'host-scene-radiance','shadowMask':'none','baseAo':'albedo-alpha'}}
            model_path.write_text(json.dumps(model))
            level_path=folder/'level.azurelevel';level=json.loads(level_path.read_text());level['renderSettings'].update(sceneLighting=True,skyIntensity=0,nprLighting=settings)
            level['lights'][0].update(color=[.8,.6,.4],intensity=.7)
            if name=='surface-only':level['lights'][0]['shape']['receivers']={'schemaVersion':1,'surfaces':True,'characters':False}
            if name in ('ambient','zero-radiance'):level['lights'][0]['enabled']=False
            if name=='additional-directional':
                extra=copy.deepcopy(level['lights'][0]);extra.update(id='additional',nodeId='additional',color=[1,1,1],intensity=.1)
                level['lights'].append(extra);level['nodes'].append({'id':'additional','components':{'azure.transform':component('azure.transform',{'translation':[0,0,2]})}})
            if name.startswith('main-shadow') or name in ('indoor-shadow','ambient-weight-shadow'):
                surface(folder/'blocker.gltf');level['resources'].append({'id':'blocker','asset':'assets:/blocker.gltf'})
                level['nodes'].append({'id':'blocker','resourceId':'blocker','components':{'azure.transform':component('azure.transform',{'translation':[1,0,1],'scale':[.6,.6,.6]})}})
                level['lights'][0]['shape']['direction']=[-2**-.5,0,-2**-.5]
                if name=='main-shadow-floor':settings['shadowOverride']=1
                if name=='indoor-shadow':settings['indoorMix']=1
                level['renderSettings']['nprLighting']=settings
            if name=='scene-volume':
                cell=[0,0,0,.4,0,0,0,.3,0,0,0,.2,1,0,0,0]
                (folder/'field.aziv').write_bytes(struct.pack('<8s4f3I',b'AZIV0001',-4,-4,-4,8,2,2,2)+struct.pack('<128f',*(cell*8)))
                level['irradianceVolume']={'version':1,'asset':'assets:/field.aziv','enabled':True,'intensity':1}
            level_path.write_text(json.dumps(level))
            command=[str(player.resolve()),'--project',str(folder/'lab.azureproject'),'--width','128','--height','128',
                '--capture-dir',str(folder/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene',
                '--qa-effect','specular','--qa-effect-state','disabled','--runtime-report',str(folder/'runtime.json')]
            if name=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (folder/'command.json').write_text(json.dumps(command,indent=2))
            with (folder/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log,name
            validation='Validation layer: enabled' in log;assert validation or not require_validation
            if expected_validation is not None:assert validation==(expected_validation=='enabled')
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text(encoding='utf-8'))
            assert manifest['nprLighting']['enabled']==settings['enabled']
            assert manifest['nprLighting']['schemaVersion']==1
            assert manifest['nprLighting']['ambientWeight']==settings.get('ambientWeight',1)
            state_hashes[name]=manifest['qaStateHash']
            frame=folder/'frames/frame_000000.png';pixels=np.asarray(Image.open(frame).convert('RGB'));frames[name]=pixels
            measured=linear(pixels[64,64]);color=reference(linear([90,80,70]),linear([60,50,40]),np.array([180,150,170,153])/255,204/255,.55)
            main=np.array([.8,.6,.4]);intensity=.7
            if name in ('ambient','zero-radiance','point-only','surface-only'):main=np.ones(3);intensity=0
            visibility=0 if name in ('main-shadow','indoor-shadow','ambient-weight-shadow') else 1
            # No character shadow mask: scene visibility selects indoor color
            # independently of the base-AO/ramp lit mask.
            lit_mask=min(204/255,153/255)
            if not visibility:
                lit=linear([90,80,70])*.96;shadow=linear([60,50,40])*.96*.55
                luminance=np.dot(lit,[.21267290413379669,.7151522040367126,.07217500358819962])
                saturated=luminance+(lit-luminance)*1.2
                color=shadow+(saturated-shadow)*(204/255)
            ambient,direct=illumination(settings,[0,0,1],[.4,.3,.2] if name=='scene-volume' else settings['ambientColor'],main,intensity,1,lit_mask,visibility)
            expected=color*(ambient+direct)
            if name=='point-only':
                position=np.array([.03125,-.03125,0]);delta=np.array([0,0,2])-position;distance=np.linalg.norm(delta)
                attenuation=(1-distance/10)**2/(1+.12*distance*distance)
                expected+=color*np.array([.8,.6,.4])*.7*attenuation*delta[2]/distance
            if name=='additional-directional':expected+=color*.1
            if name=='disabled':expected=color*main*intensity
            error=float(np.abs(measured-expected).max())
            summary['captures'].append({'name':name,'maximumLinearError':error,'validationEnabled':validation,
                'imageSha256':hashlib.sha256(frame.read_bytes()).hexdigest(),'measured':measured.tolist(),'expected':expected.tolist(),
                'allocatorAfterUnload':{'buffers':0,'images':0}})
            if error is not None:assert error<=.008,(name,error,measured,expected)
        if 'composed' in frames and 'fixed' in frames:assert np.array_equal(frames['composed'],frames['fixed'])
        if 'composed' in frames and 'disabled' in frames:assert np.max(np.abs(linear(frames['composed'][64,64])-linear(frames['disabled'][64,64])))>.01
        if 'direct' in frames and 'ambient-weight-direct' in frames:assert np.array_equal(frames['direct'],frames['ambient-weight-direct'])
        if 'zero-radiance' in frames:assert np.max(linear(frames['zero-radiance'][64,64]))==0
        if 'composed' in state_hashes and 'disabled' in state_hashes:assert state_hashes['composed']!=state_hashes['disabled']
        if 'main-shadow' in frames and 'main-shadow-floor' in frames:assert np.max(np.abs(linear(frames['main-shadow'][64,64])-linear(frames['main-shadow-floor'][64,64])))>.01
        summary['status']='passed'
    except Exception as error:summary.update(status='failed',error=str(error));raise
    finally:(output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path);parser.add_argument('--require-validation',action='store_true')
    parser.add_argument('--case',action='append')
    parser.add_argument('--expected-validation',choices=('enabled','disabled'))
    args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.require_validation,args.case,args.expected_validation)
    else:
        runs=Path(__file__).resolve().parents[1]/'build/runs';runs.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='npr-illumination-',dir=runs) as temporary:
            run(args.executable,Path(temporary)/'capture',args.require_validation,args.case,args.expected_validation)
