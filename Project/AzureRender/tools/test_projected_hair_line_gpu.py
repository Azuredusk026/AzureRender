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


def run(player, output):
    output.mkdir(parents=True, exist_ok=False)
    summary={'schemaVersion':1,'status':'running','playerSha256':hashlib.sha256(player.read_bytes()).hexdigest(),
        'linearErrorBudget':ramp.BUDGET,'captures':[],
        'scope':'Material line response, constant irradiance, actual upload and descriptor paths; native shadow-field adaptation is separate.'}
    measured_cases={}
    names=('control','enabled','fixed','depth-zero','g-zero','alternating-low','alternating-high',
        'uv-low','uv-high','shift','range','virtual','point-only','no-light','direct-kk')
    try:
        for name in names:
            print(name,flush=True)
            root=output/name;case=copy.deepcopy(ramp.cases()[0]);case['name']=name
            case.update(fixed=name=='fixed',local=name=='point-only',dark=name=='no-light')
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
            if name=='depth-zero':line['depth']=0
            if name.startswith('alternating-'):line.update(alternation=0,frequency=.8 if name.endswith('low') else 1.2)
            if name=='uv-high':line['uvTransform']=[1,1,.375,0]
            if name in ('enabled','fixed','shift','range','virtual','point-only','no-light','direct-kk','g-zero','depth-zero'):
                line['uvTransform']=[1,1,.125,0]  # texel centre .625 = 128/255
            if name=='shift':line['strandShift']=1
            if name=='range':line.update(range=0,strandShift=1)
            for material in asset['materials']:
                profile=material['extras']['azureRenderMaterial']['hair']['projectedRamp']
                profile.update(surfaceResponse='strand-diffuse',diffuseScale=.96)
                if name!='control':profile['line']=line
            asset_path.write_text(json.dumps(asset),encoding='utf-8')
            level_path=root/'assets/scene.azurelevel';level=json.loads(level_path.read_text())
            cell=[0,0,0,.4,0,0,0,.3,0,0,0,.2,1,0,0,0]
            (root/'assets/field.aziv').write_bytes(struct.pack('<8s4f3I',b'AZIV0001',-4,-4,-4,8,2,2,2)+struct.pack('<128f',*(cell*8)))
            level['irradianceVolume']={'version':1,'asset':'assets:/field.aziv','enabled':True,'intensity':1}
            level_path.write_text(json.dumps(level),encoding='utf-8')
            wanted=[]
            for sample in expected:
                side=sample['side'];point=np.asarray(sample['worldPoint']);view=ramp.normalized(ramp.CAMERA-point)
                light=np.array([0,case.get('lightY',.04),np.sqrt(1-case.get('lightY',.04)**2)])
                half=ramp.virtual_half_vector(view=view,light=light,object_x=(1,0,0),object_y=(0,1,0),object_z=(0,0,1),blend=1) if name=='virtual' else ramp.normalized(view+light)
                parameters=(case[side]['r']/255,case[side]['g']/255,37/255,0)
                coordinates=ramp.project_coordinates(half_vector=half,highlight_normal=(0,0,1),view=view,
                    source_parameters=parameters,tangent=(1,0,0,1),object_x=(1,0,0),object_y=(0,1,0),object_z=(0,0,1),
                    primary_shift=case.get('primaryShift',0),secondary_shift=0,range2=1,edge_fade_power=2,direction_x=0)
                energy=evaluate_energy(coordinates,ramp.ramp_sample(coordinates.uv)[0],parameters,1,(0,0,.5))
                rgb=ramp.linear(ramp.CONTROL[:3])*.96*np.array([.4,.3,.2])
                if name not in ('control','point-only','no-light'):
                    sample_r=values[round((.5*line['uvTransform'][0]+line['uvTransform'][2])*4-.5)%4]/255
                    factor,saturation=line_response(strand=(0,2*parameters[0]-1,0),normal=(0,0,1),half_vector=half,
                        u=.5,frequency=line['frequency'],strand_shift=line['strandShift'],range1=line['range'],depth=line['depth'],
                        alternation=line['alternation'],line_sample=sample_r,parameter_g=parameters[1],q=energy.q,saturation=line['saturation'])
                    dark=rgb*factor;rgb=np.dot(dark,[.212673,.715152,.072175])*(1-saturation)+dark*saturation
                if name=='direct-kk':rgb=np.asarray(sample['linearRgb'])
                wanted.append(rgb)
            command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128',
                '--capture-dir',str(root/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene',
                '--qa-isolation','hair-kk' if name=='direct-kk' else 'ambient','--runtime-report',str(root/'runtime.json')]
            if name=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (root/'command.json').write_text(json.dumps(command,indent=2),encoding='utf-8')
            with (root/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(root/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation Error|Validation Warning|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log,name
            assert 'Validation layer: enabled' in log,name
            runtime=json.loads((root/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
            frame=root/'frames/frame_000000.png';pixels=np.asarray(Image.open(frame).convert('RGB'))
            measured=np.asarray([ramp.linear(pixels[y,x]) for _,x,y in ramp.samples(case)])
            error=float(np.abs(measured-np.asarray(wanted)).max());measured_cases[name]=measured
            summary['captures'].append({'name':name,'maximumLinearError':error,'validationEnabled':True,
                'allocatorAfterUnload':{'buffers':0,'images':0},'imageSha256':hashlib.sha256(frame.read_bytes()).hexdigest()})
            (root/'expected.json').write_text(json.dumps({'linearRgb':np.asarray(wanted).tolist(),'measured':measured.tolist()},indent=2))
            assert error<=ramp.BUDGET,(name,error)
        assert np.array_equal(measured_cases['enabled'],measured_cases['fixed'])
        assert np.max(np.abs(measured_cases['enabled']-measured_cases['control']))>.01
        assert np.max(np.abs(measured_cases['uv-low']-measured_cases['uv-high']))>.01
        assert np.max(np.abs(measured_cases['alternating-low']-measured_cases['alternating-high']))>.01
        summary['status']='passed'
    except Exception as error:
        summary.update(status='failed',error=str(error));raise
    finally:
        (output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();run(args.executable,args.output.resolve())
