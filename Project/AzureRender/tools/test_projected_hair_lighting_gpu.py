"""Check projected hair color and both lit lobes through public material inputs."""
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
import test_projected_hair_gpu as hair
from test_npr_illumination_gpu import illumination
from test_character_local_light_gpu import line_reference

LUMA=np.array([.21267290413379669,.7151522040367126,.07217500358819962])
SETTINGS=dict(schemaVersion=1,enabled=True,ambientColor=[.1,.1,.1],ambientDirection=[0,1,0],
    directionBias=.15,directionGain=1.5,directionFloor=.5,outdoorGain=1.05,shadowAmbientGain=.8,
    indoorMix=0,litAmbientMix=1,mainColorMix=1,shadowOverride=0,useSceneIrradiance=False,ambientWeight=.2877224684)
LIGHTING=dict(schemaVersion=1,shadowBrightness=.55,shadowSaturation=1.1,shadowScale=.55,
    ambientColor=[.8783,.9302,1.1217],mainColor=[.8,.7,.6])


def saturation(color,amount):
    return np.dot(color,LUMA)+(color-np.dot(color,LUMA))*amount


def color_response(base,ramp,visibility,view_alpha,scale,brightness,sat):
    lit=base*.96;shadow=saturation(base*brightness,sat)*(.96*scale)
    lit_mask=min(visibility,ramp[3]);ao=visibility
    shadow_color=saturation(shadow*.65,1.2)
    shadow_color+=(shadow-shadow_color)*np.clip(view_alpha*ao+ramp[3],0,1)
    untinted=shadow_color+(lit-shadow_color)*lit_mask
    chroma=max(ramp[:3])-min(ramp[:3]);tinted=untinted*(1-chroma+ramp[:3]*chroma)
    return tinted*np.clip(np.dot(untinted,LUMA)/max(np.dot(tinted,LUMA),.001),0,1.5),lit_mask


def run(player,output,selected=None,expected_validation=None):
    output.mkdir(parents=True,exist_ok=False)
    names=('direct','ambient','dim-visibility','diffuse-scale','kk','kk-fixed','kk-zero-ambient','kk-disabled','local-kk','extra-kk','no-main','direct-fixed','disabled-lighting','absent-input','local-response-kk','local-response-zero-kk','local-line-kk','local-line-kk-fixed')
    if selected:
        assert set(selected)<=set(names);names=tuple(n for n in names if n in selected)
    report={'schemaVersion':1,'status':'running','playerSha256':hashlib.sha256(player.read_bytes()).hexdigest(),'captures':[]}
    frames={}
    try:
        for name in names:
            print(name,flush=True);folder=output/name;case=copy.deepcopy(hair.cases()[0]);case['name']=name
            # Both panels supply a primary lobe; the left also supplies a blue secondary lobe.
            case['left'].update(r=0,g=128,a=128);case['right'].update(r=255,g=128,a=0)
            project,_=hair.prepare(case,folder);model_path=folder/'assets/panels.gltf';model=json.loads(model_path.read_text())
            ramp=np.array([180,150,170,153])/255;vis=12 if name=='dim-visibility' else 225
            model['images'].append({'uri':hair.image_uri(Image.new('RGBA',(2,1),tuple((ramp*255).astype(int))))})
            model['textures'].append({'source':len(model['images'])-1});ramp_index=len(model['textures'])-1
            for material in model['materials']:
                p=material['extras']['azureRenderMaterial'];p['features'].append('diffuse-ramp')
                p['diffuseRamp']={'schemaVersion':1,'texture':ramp_index};p['styleParameters']=[1,1,0,0]
                h=p['hair']['projectedRamp'];h.update(surfaceResponse='strand-diffuse',diffuseScale=.48 if name=='diffuse-scale' else .96,lighting=LIGHTING)
                if name=='absent-input':del h['lighting']
                image=model['textures'][h['parameterTexture']]['source'];side=case[material['name']]
                model['images'][image]['uri']=hair.image_uri(Image.new('RGBA',(2,2),(side['r'],side['g'],vis,side['a'])))
            model_path.write_text(json.dumps(model),encoding='utf-8')
            level_path=folder/'assets/scene.azurelevel';level=json.loads(level_path.read_text());settings=copy.deepcopy(SETTINGS)
            if name=='kk-zero-ambient':settings['ambientWeight']=0
            level['renderSettings']['nprLighting']=settings;level['lights'][0]['color']=[.2,.4,.6]
            if name=='disabled-lighting':settings['enabled']=False
            if name=='no-main':level['lights'][0]['enabled']=False
            if name.startswith('local-'):level['lights'][0]['shape']['type']='point'
            if name.startswith('local-response-'):
                level['lights'][0]['intensity']=4
                level['lights'][0]['shape'].update(characterResponse={'schemaVersion':1,'diffuseFloor':.25,'compression':1},
                    specularScale=0 if 'zero' in name else 2)
            if name.startswith('local-line-'):
                level['lights'][0]['intensity']=8
                level['lights'][0]['shape'].update(direction=[1,0,0],falloff='smooth-inverse-square',specularScale=2,
                    characterResponse={'schemaVersion':2,'diffuseFloor':.25,'compression':0,'linearLength':4})
            if name=='extra-kk':
                light=copy.deepcopy(level['lights'][0]);light.update(id='extra',color=[.1,.2,.3],intensity=.2);level['lights'].append(light)
            level_path.write_text(json.dumps(level),encoding='utf-8')
            isolation='hair-kk' if 'kk' in name or name=='no-main' else 'ambient' if name=='ambient' else 'direct-diffuse'
            command=[str(player.resolve()),'--project',str(project),'--width','128','--height','128','--capture-dir',str(folder/'frames'),
                '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation',isolation]
            if name.endswith('fixed'):command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            if name=='kk-disabled':command+=['--qa-effect','hair-kk','--qa-effect-state','disabled']
            (folder/'command.json').write_text(json.dumps(command,indent=2),encoding='utf-8')
            with (folder/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log,name
            validation='Validation layer: enabled' in log
            if expected_validation:assert validation==(expected_validation=='enabled')
            frame=folder/'frames/frame_000000.png';pixels=np.asarray(Image.open(frame).convert('RGB'));frames[name]=pixels
            measured=[];wanted=[]
            energy_case=copy.deepcopy(case)
            if name.startswith('local-'):energy_case['local']=True
            energy_samples=hair.expectation(energy_case)
            for sample in energy_samples:
                x,y=sample['pixel'];measured.append(hair.linear(pixels[y,x]))
                response,lit=color_response(hair.linear(hair.CONTROL[:3]),ramp,vis/255,ramp[3],.55,.55,1.1)
                main=LIGHTING['mainColor'];intensity=0 if name=='no-main' or name.startswith('local-') else .5
                ambient,direct=illumination(settings,[0,0,1],LIGHTING['ambientColor'],main,intensity,1,lit,1)
                if isolation=='hair-kk':
                    weight=lit;spec_scale=(.55+(1-.55)*weight)*(.5+.5*weight)
                    raw=np.asarray(sample['linearRgb'])/(.5*3)
                    expected=raw*(ambient+direct)*spec_scale*3
                    if name in ('no-main','kk-disabled'):expected*=0
                    if name.startswith('local-'):expected=np.asarray(sample['linearRgb'])*[.2,.4,.6]
                    if name.startswith('local-response-'):
                        delta=np.array([0,.16,4])-sample['worldPoint'];distance=np.linalg.norm(delta);cosine=delta[2]/distance
                        attenuation=(1-distance/10)**2/(1+.12*distance*distance)
                        denominator=max(.6*4*attenuation*.75,1)
                        expected*=np.clip(cosine+.5,0,1)*.75+.25
                        expected*=8*cosine*(0 if 'zero' in name else 2)/denominator
                    if name.startswith('local-line-'):
                        point=np.asarray(sample['worldPoint']);delta=np.array([0,.16,4])-point
                        direction,factor=line_reference(delta,[1,0,0],4);view=hair.normalized(hair.CAMERA-point)
                        side=case[sample['side']];parameters=(side['r']/255,side['g']/255,vis/255,side['a']/255)
                        coordinates=hair.project_coordinates(half_vector=hair.normalized(view+direction),highlight_normal=(0,0,1),
                            view=view,source_parameters=parameters,tangent=(1,0,0,1),object_x=(1,0,0),object_y=(0,1,0),object_z=(0,0,1),
                            primary_shift=0,secondary_shift=0,range2=1,edge_fade_power=2,direction_x=0)
                        sampled,_=hair.ramp_sample(coordinates.uv);primary=hair.evaluate_energy(coordinates,sampled,parameters,1,(0,0,.5)).primary
                        q=np.dot(delta,delta)/100;incident=np.array([.2,.4,.6])*8*factor*max(1-q*q,0)**2
                        incident*=.25+np.clip(direction[2]+.5,0,1)*.75
                        expected=np.asarray(primary)*incident*max(direction[2],0)*2*3
                    if name=='extra-kk':expected+=raw*np.array([.1,.2,.3])*.2*3
                else:expected=response*(ambient if isolation=='ambient' else direct)*(.5 if name=='diffuse-scale' else 1)
                if name in ('disabled-lighting','absent-input'):expected=hair.linear(hair.CONTROL[:3])*.96*np.array([.2,.4,.6])*.5*ramp[:3]
                wanted.append(expected)
            maximum=float(np.abs(np.asarray(measured)-wanted).max())
            if name=='local-response-kk' or name.startswith('local-line-'):assert np.max(wanted)>.015,'KK positive control must exceed quantization and comparison budget'
            report['captures'].append({'name':name,'maximumLinearError':maximum,'imageSha256':hashlib.sha256(frame.read_bytes()).hexdigest(),
                'validationEnabled':validation,'measured':measured[0].tolist(),'expected':np.asarray(wanted[0]).tolist()})
            assert maximum<=.008,(name,maximum,measured[0],wanted[0])
        for a,b in (('direct','direct-fixed'),('kk','kk-fixed'),('local-line-kk','local-line-kk-fixed')):
            if a in frames and b in frames:assert np.array_equal(frames[a],frames[b])
        if 'disabled-lighting' in frames and 'absent-input' in frames:assert np.array_equal(frames['disabled-lighting'],frames['absent-input'])
        report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path)
    p.add_argument('--case',action='append');p.add_argument('--expected-validation',choices=('enabled','disabled'));args=p.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.case,args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='projected-lighting-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(args.executable,Path(temporary)/'capture',args.case,args.expected_validation)
