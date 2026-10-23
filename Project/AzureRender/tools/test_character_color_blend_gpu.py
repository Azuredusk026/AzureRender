"""Verify ordered character color composition through public scene documents."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
from PIL import Image
from native_render.light_lab import generate
from test_npr_diffuse_gpu import linear


def run(player,output,selected=None,expected_validation=None):
    output.mkdir(parents=True,exist_ok=False)
    names=('single','zero','full','facing-front','facing-back','two-layers','reverse-layers','fixed',
        'projected','projected-behind','sphere','geometric-normal','pbr','pbr-control','disabled','disabled-control','no-receiver','ambient','ambient-control','ambient-direct','ambient-indirect')
    if selected:
        assert set(selected)<=set(names);names=tuple(n for n in names if n in selected)
    report=dict(schemaVersion=1,status='running',playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(),captures=[]);frames={}
    try:
        for name in names:
            print(name,flush=True);folder=output/name;generate(folder,'dark')
            model_path=folder/'plane.gltf';model=json.loads(model_path.read_text());mat=model['materials'][0]
            mat['pbrMetallicRoughness'].update(baseColorFactor=[.25,.25,.25,1],metallicFactor=0,roughnessFactor=1)
            Image.new('RGBA',(2,1),(255,255,255,255)).save(folder/'ramp.png')
            Image.new('RGBA',(4,2),(128,128,128,255)).save(folder/'lut.png')
            model.update(images=[dict(uri='ramp.png'),dict(uri='lut.png')],textures=[dict(source=0),dict(source=1)])
            mat['extras']={'azureRenderMaterial':{'schemaVersion':2,'class':'skin','features':['diffuse-ramp'],
                'diffuseRamp':{'schemaVersion':1,'texture':0},'shadowColorLut':{'schemaVersion':1,'texture':1,'flipV':False},'styleParameters':[1,1,0,0],
                'nprDiffuse':{'schemaVersion':1,'shadowScale':1,'lightingInputs':'host-scene-radiance','shadowMask':'none','baseAo':'one'}}}
            if name=='geometric-normal':
                Image.new('RGBA',(1,1),(255,128,128,255)).save(folder/'normal.png')
                model['images'].append(dict(uri='normal.png'));model['textures'].append(dict(source=2));mat['normalTexture']=dict(index=2)
            if name.startswith('pbr'):mat.pop('extras')
            model_path.write_text(json.dumps(model),encoding='utf-8')
            path=folder/'level.azurelevel';level=json.loads(path.read_text());level['lights']=[]
            settings=level['renderSettings'];settings['nprLighting']=dict(schemaVersion=1,enabled=not name.startswith('disabled'),
                ambientColor=[.8,.6,.4] if name.startswith('ambient') else [0,0,0],ambientDirection=[0,1,0],
                directionBias=0,directionGain=0,directionFloor=1,outdoorGain=1,shadowAmbientGain=1,indoorMix=0,
                litAmbientMix=0,mainColorMix=0,shadowOverride=1,useSceneIrradiance=False)
            colors=[[.6,.2,.1]];strengths=[0 if name=='zero' else 1 if name=='full' else .7]
            if name in ('two-layers','reverse-layers'):colors=[[.6,.2,.1],[.1,.4,.8]];strengths=[.7,.5]
            for i,(color,strength) in enumerate(zip(colors,strengths)):
                projected=name.startswith('projected');position=[-1,0,100] if name=='projected-behind' else [3,0,1] if name in ('projected','sphere') else [0,0,-1] if name=='facing-back' else [0,0,1]
                if name=='full':position=[.03125,-.03125,0]
                response=dict(schemaVersion=3,composition='color-blend',strength=strength,
                    facingWeight=1 if name in ('facing-front','facing-back','geometric-normal') else 0,
                    projectedDistance=projected,order=1-i if name=='reverse-layers' else i)
                shape=dict(type='spot' if projected else 'point',direction=[1,0,0] if name=='projected-behind' else [0,0,-1],falloff='range-power',falloffExponent=2,
                    innerCone=.25,outerCone=.5,characterResponse=response)
                if name=='no-receiver':shape['receivers']=dict(schemaVersion=1,surfaces=True,characters=False)
                identifier='modifier-'+str(i)
                level['nodes'].append(dict(id=identifier,components={'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=position))}))
                level['lights'].append(dict(id=identifier,nodeId=identifier,enabled=True,color=color,intensity=1,radius=2,shape=shape))
            if name.endswith('control'):level['lights']=[]
            if name.startswith(('pbr','disabled')):
                level['lights'].append(dict(id='physical',nodeId='light',enabled=True,color=[.8,.6,.4],intensity=.7,radius=10,
                    shape=dict(type='point',characterResponse=dict(schemaVersion=1,diffuseFloor=.25,compression=1))))
            path.write_text(json.dumps(level),encoding='utf-8')
            command=[str(player.resolve()),'--project',str(folder/'lab.azureproject'),'--width','128','--height','128',
                '--capture-dir',str(folder/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene']
            if name=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            if name in ('ambient-direct','ambient-indirect'):command+=['--qa-isolation','direct-diffuse' if name=='ambient-direct' else 'ambient']
            (folder/'command.json').write_text(json.dumps(command,indent=2),encoding='utf-8')
            with (folder/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log
            validation='Validation layer: enabled' in log
            if expected_validation:assert validation==(expected_validation=='enabled')
            frame=folder/'frames/frame_000000.png';pixels=np.asarray(Image.open(frame).convert('RGB'));frames[name]=pixels
            measured=linear(pixels[64,64]);expected=np.zeros(3)
            # Shared character diffuse uses the authored default .96 scale.
            if name.startswith('ambient'):expected=.96*np.array([.25,.25,.25])*[.8,.6,.4]
            if name=='ambient-direct':expected[:]=0
            for light in sorted(level['lights'],key=lambda v:v['shape']['characterResponse'].get('order',0)):
                if name.startswith(('pbr','disabled')) or name=='no-receiver':continue
                response=light['shape']['characterResponse'];position=next(n for n in level['nodes'] if n['id']==light['nodeId'])['components']['azure.transform']['data']['translation']
                incoming=np.array(position)-[.03125,-.03125,0]
                if response['projectedDistance']:
                    axis=np.asarray(light['shape']['direction']);incoming=axis*np.dot(incoming,axis)
                squared=np.dot(incoming,incoming);direction=incoming/max(np.sqrt(squared),.001)
                window=max(1-min(squared/4,1),0)**2
                facing=np.clip(direction[2]+.5,0,1);smooth=facing*facing*(3-2*facing)
                weight=window*response['strength']*(1-response['facingWeight']+response['facingWeight']*smooth)
                expected=expected*(1-weight)+(np.zeros(3) if name=='ambient-indirect' else np.array(light['color']))*weight
            error=None if name.startswith(('pbr','disabled')) else float(np.abs(measured-expected).max())
            if error is None:assert max(measured)>.015,'Compatibility controls must remain visibly lit'
            else:assert error<=.008,(name,error,measured,expected)
            report['captures'].append(dict(name=name,maximumLinearError=error,measured=measured.tolist(),expected=None if error is None else expected.tolist(),
                imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest(),validationEnabled=validation))
        for a,b in (('single','fixed'),('facing-front','geometric-normal'),('pbr','pbr-control'),('disabled','disabled-control')):
            if a in frames and b in frames:assert np.array_equal(frames[a],frames[b]),(a,b)
        if all(n in frames for n in ('two-layers','reverse-layers')):
            assert np.max(np.abs(linear(frames['two-layers'][64,64])-linear(frames['reverse-layers'][64,64])))>.02
        report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path)
    p.add_argument('--case',action='append');p.add_argument('--expected-validation',choices=('enabled','disabled'));a=p.parse_args()
    if a.output:run(a.executable,a.output.resolve(),a.case,a.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='color-blend-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(a.executable,Path(temporary)/'capture',a.case,a.expected_validation)
