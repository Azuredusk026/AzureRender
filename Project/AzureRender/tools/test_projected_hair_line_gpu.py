"""Validate captured strand-line modulation through actual material uploads."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

import numpy as np
from PIL import Image
import test_projected_hair_gpu as ramp
from endfield_native.projected_hair import evaluate_energy, line_response
from test_projected_hair_surface_gpu import expected_diffuse


def run(player, output, selected=None, expected_validation="enabled"):
    output.mkdir(parents=True, exist_ok=False)
    summary={'schemaVersion':1,'status':'running','playerSha256':hashlib.sha256(player.read_bytes()).hexdigest(),
        'linearErrorBudget':ramp.BUDGET,'captures':[],
        'scope':'Material line response, constant irradiance, actual upload and descriptor paths; native shadow-field adaptation is separate.'}
    measured_cases={}
    names=('control','enabled','fixed','depth-zero','g-zero','alternating-low','alternating-high',
        'uv-low','uv-high','shift','range','virtual','point-only','no-light','direct-kk',
        'direct-control','combined-direct','combined-fixed','combined-ambient','combined-point','combined-kk','local-control','combined-local')
    if selected:
        assert set(selected)<=set(names);names=tuple(name for name in names if name in selected)
    try:
        for name in names:
            print(name,flush=True)
            root=output/name;case=copy.deepcopy(ramp.cases()[0]);case['name']=name
            case.update(fixed=name in ('fixed','combined-fixed'),local=name in ('point-only','combined-point'),dark=name=='no-light')
            if name=='g-zero':
                for side in ('left','right'):case[side]['g']=0
            if name=='virtual':case.update(virtual=True,objectGain=1,lightY=.6,primaryShift=-1/3)
            project,expected=ramp.prepare(case,root)
            asset_path=root/'assets/panels.gltf';asset=json.loads(asset_path.read_text())
            values=(0,64,128,255)
            image=Image.new('RGBA',(4,1));image.putdata([(value,value,value,255) for value in values])
            asset['images'].append({'uri':ramp.image_uri(image)})
            asset['textures'].append({'source':len(asset['images'])-1})
            line=dict(schemaVersion=1,texture=len(asset['textures'])-1,frequency=.8,strandShift=0,range=1,
                depth=.8,alternation=1,saturation=1.5,uvTransform=[1,1,-.375,0])
            if name.startswith('combined-'):line['diffuseTarget']='main-and-ambient'
            if name=='depth-zero':line['depth']=0
            if name.startswith('alternating-'):line.update(alternation=0,frequency=.8 if name.endswith('low') else 1.2)
            if name=='uv-high':line['uvTransform']=[1,1,.375,0]
            if name.startswith('combined-') or name in ('enabled','fixed','shift','range','virtual','point-only','no-light','direct-kk','g-zero','depth-zero'):
                line['uvTransform']=[1,1,.125,0]  # texel centre .625 = 128/255
            if name=='shift':line['strandShift']=1
            if name=='range':line.update(range=0,strandShift=1)
            for material in asset['materials']:
                if name in ('direct-control','combined-direct','combined-fixed','combined-point','local-control','combined-local'):
                    material['extras']['azureRenderMaterial']['styleParameters']=[0,0,1,0]
                profile=material['extras']['azureRenderMaterial']['hair']['projectedRamp']
                profile.update(surfaceResponse='strand-diffuse',diffuseScale=.96)
                if name not in ('control','direct-control','local-control'):profile['line']=line
            asset_path.write_text(json.dumps(asset),encoding='utf-8')
            level_path=root/'assets/scene.azurelevel';level=json.loads(level_path.read_text())
            cell=[0,0,0,.4,0,0,0,.3,0,0,0,.2,1,0,0,0]
            (root/'assets/field.aziv').write_bytes(struct.pack('<8s4f3I',b'AZIV0001',-4,-4,-4,8,2,2,2)+struct.pack('<128f',*(cell*8)))
            if name in ('local-control','combined-local'):
                point_light=copy.deepcopy(level['lights'][0]);point_light.update(id='local',nodeId='local');point_light['shape']={'type':'point','falloff':'legacy'}
                level['lights'].append(point_light)
                point_node=copy.deepcopy(next(node for node in level['nodes'] if node['id']=='light'));point_node['id']='local';level['nodes'].append(point_node)
            level['irradianceVolume']={'version':1,'asset':'assets:/field.aziv','enabled':True,'intensity':1}
            level_path.write_text(json.dumps(level),encoding='utf-8')
            wanted=[]
            direct_case=name in ('direct-control','combined-direct','combined-fixed','combined-point','local-control','combined-local')
            direct=expected_diffuse(case,scale=.96) if direct_case else None
            extra_point=expected_diffuse(dict(case,local=True),scale=.96) if name in ('local-control','combined-local') else None
            for sample_index,sample in enumerate(expected):
                side=sample['side'];point=np.asarray(sample['worldPoint']);view=ramp.normalized(ramp.CAMERA-point)
                light=np.array([0,case.get('lightY',.04),np.sqrt(1-case.get('lightY',.04)**2)])
                half=ramp.virtual_half_vector(view=view,light=light,object_x=(1,0,0),object_y=(0,1,0),object_z=(0,0,1),blend=1) if name=='virtual' else ramp.normalized(view+light)
                parameters=(case[side]['r']/255,case[side]['g']/255,37/255,0)
                coordinates=ramp.project_coordinates(half_vector=half,highlight_normal=(0,0,1),view=view,
                    source_parameters=parameters,tangent=(1,0,0,1),object_x=(1,0,0),object_y=(0,1,0),object_z=(0,0,1),
                    primary_shift=case.get('primaryShift',0),secondary_shift=0,range2=1,edge_fade_power=2,direction_x=0)
                energy=evaluate_energy(coordinates,ramp.ramp_sample(coordinates.uv)[0],parameters,1,(0,0,.5))
                rgb=direct[sample_index] if direct_case else ramp.linear(ramp.CONTROL[:3])*.96*np.array([.4,.3,.2])
                if name not in ('control','direct-control','local-control','point-only','combined-point','no-light'):
                    sample_r=values[round((.5*line['uvTransform'][0]+line['uvTransform'][2])*4-.5)%4]/255
                    factor,saturation=line_response(strand=(0,2*parameters[0]-1,0),normal=(0,0,1),half_vector=half,
                        u=.5,frequency=line['frequency'],strand_shift=line['strandShift'],range1=line['range'],depth=line['depth'],
                        alternation=line['alternation'],line_sample=sample_r,parameter_g=parameters[1],q=energy.q,saturation=line['saturation'])
                    dark=rgb*factor;rgb=np.dot(dark,[.212673,.715152,.072175])*(1-saturation)+dark*saturation
                if extra_point is not None:rgb=rgb+extra_point[sample_index]
                if name in ('direct-kk','combined-kk'):rgb=np.asarray(sample['linearRgb'])
                wanted.append(rgb)
            command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128',
                '--capture-dir',str(root/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene',
                '--qa-isolation','hair-kk' if name in ('direct-kk','combined-kk') else 'direct-diffuse' if direct_case else 'ambient','--runtime-report',str(root/'runtime.json')]
            if case['fixed']:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (root/'command.json').write_text(json.dumps(command,indent=2),encoding='utf-8')
            with (root/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(root/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation Error|Validation Warning|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log,name
            validation='Validation layer: enabled' in log
            assert validation==(expected_validation=='enabled'),name
            runtime=json.loads((root/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
            frame=root/'frames/frame_000000.png';pixels=np.asarray(Image.open(frame).convert('RGB'))
            measured=np.asarray([ramp.linear(pixels[y,x]) for _,x,y in ramp.samples(case)])
            error=float(np.abs(measured-np.asarray(wanted)).max());measured_cases[name]=measured
            summary['captures'].append({'name':name,'maximumLinearError':error,'validationEnabled':validation,
                'allocatorAfterUnload':{'buffers':0,'images':0},'imageSha256':hashlib.sha256(frame.read_bytes()).hexdigest()})
            (root/'expected.json').write_text(json.dumps({'linearRgb':np.asarray(wanted).tolist(),'measured':measured.tolist()},indent=2))
            assert error<=ramp.BUDGET,(name,error)
        for left,right in [('enabled','fixed'),('combined-direct','combined-fixed'),('enabled','combined-ambient'),('direct-kk','combined-kk')]:
            if left in measured_cases and right in measured_cases:assert np.array_equal(measured_cases[left],measured_cases[right]),(left,right)
        for left,right in [('enabled','control'),('uv-low','uv-high'),('alternating-low','alternating-high'),('direct-control','combined-direct'),('local-control','combined-local')]:
            if left in measured_cases and right in measured_cases:assert np.max(np.abs(measured_cases[left]-measured_cases[right]))>.01,(left,right)
        summary['status']='passed'
    except Exception as error:
        summary.update(status='failed',error=str(error));raise
    finally:
        (output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--case',action='append');parser.add_argument('--expected-validation',choices=('enabled','disabled'),default='enabled')
    args=parser.parse_args();run(args.executable,args.output.resolve(),args.case,args.expected_validation)
