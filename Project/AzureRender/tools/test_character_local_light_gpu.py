"""Measure explicit character local-light color through production scene inputs."""
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
from test_npr_diffuse_gpu import linear,reference


def run(player,output,selected=None,expected_validation=None):
    output.mkdir(parents=True,exist_ok=False)
    names=('front','side','back','compressed','uncompressed','two-lights','fixed','disabled-scene-npr','no-receiver','pbr','pbr-control','upper-light','lower-light')
    if selected:
        assert set(selected)<=set(names);names=tuple(n for n in names if n in selected)
    report={'schemaVersion':1,'status':'running','playerSha256':hashlib.sha256(player.read_bytes()).hexdigest(),'captures':[]};frames={}
    try:
        for name in names:
            print(name,flush=True);folder=output/name;generate(folder,'point')
            model_path=folder/'plane.gltf';model=json.loads(model_path.read_text())
            for key,color,size in [('base',(90,80,70,204),(1,1)),('lut',(60,50,40,255),(4,2)),('ramp',(180,150,170,153),(2,1))]:Image.new('RGBA',size,color).save(folder/(key+'.png'))
            model.update(images=[{'uri':p+'.png'} for p in ('base','lut','ramp')],textures=[{'source':i} for i in range(3)])
            mat=model['materials'][0];mat['pbrMetallicRoughness'].update(baseColorTexture={'index':0},baseColorFactor=[1,1,1,1])
            mat['extras']={'azureRenderMaterial':{'schemaVersion':2,'class':'skin','features':['diffuse-ramp'],
                'shadowColorLut':{'schemaVersion':1,'texture':1,'flipV':False},'diffuseRamp':{'schemaVersion':1,'texture':2},'styleParameters':[1,1,0,0],
                'nprDiffuse':{'schemaVersion':1,'shadowScale':.55,'lightingInputs':'host-scene-radiance','shadowMask':'none','baseAo':'albedo-alpha'}}}
            if name.startswith('pbr'):del mat['extras']
            model_path.write_text(json.dumps(model),encoding='utf-8')
            path=folder/'level.azurelevel';level=json.loads(path.read_text());light=level['lights'][0]
            position=[2,0,0] if name=='side' else [0,0,-2] if name=='back' else [0,0,2]
            if name in ('upper-light','lower-light'):position=[0,2 if name=='upper-light' else -2,.5];light['radius']=1
            level['nodes'][2]['components']['azure.transform']['data']['translation']=position
            strong=name in ('compressed','uncompressed','two-lights');color=[8,4,2] if strong else [.8,.6,.4];intensity=1 if strong else 3 if name in ('side','back','upper-light','lower-light') else .7
            light.update(color=color,intensity=intensity)
            compression=0 if name=='uncompressed' else 1
            if name!='pbr-control':light['shape']['characterResponse']={'schemaVersion':1,'diffuseFloor':.25,'compression':compression}
            if name=='no-receiver':light['shape']['receivers']={'schemaVersion':1,'surfaces':True,'characters':False}
            if name=='two-lights':
                fill=copy.deepcopy(light);fill.update(id='fill',intensity=.25);level['lights'].append(fill)
            level['renderSettings']['nprLighting']={'schemaVersion':1,'enabled':name!='disabled-scene-npr','ambientColor':[0,0,0],
                'ambientDirection':[0,1,0],'directionBias':.15,'directionGain':1.5,'directionFloor':.5,'outdoorGain':1.05,
                'shadowAmbientGain':.8,'indoorMix':0,'litAmbientMix':0,'mainColorMix':1,'shadowOverride':0,'useSceneIrradiance':False}
            path.write_text(json.dumps(level),encoding='utf-8')
            command=[str(player.resolve()),'--project',str(folder/'lab.azureproject'),'--width','128','--height','128',
                '--capture-dir',str(folder/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','direct-diffuse']
            if name=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (folder/'command.json').write_text(json.dumps(command,indent=2),encoding='utf-8')
            with (folder/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log
            validation='Validation layer: enabled' in log
            if expected_validation:assert validation==(expected_validation=='enabled')
            frame=folder/'frames/frame_000000.png';pixels=np.asarray(Image.open(frame).convert('RGB'));frames[name]=pixels
            pixel_y=32 if name=='upper-light' else 96 if name=='lower-light' else 64
            measured=linear(pixels[pixel_y,64]);expected=None;error=None
            if not name.startswith('pbr'):
                delta=np.asarray(position)-[.03125,4-(pixel_y+.5)/128*8,0];distance=np.linalg.norm(delta);cosine=delta[2]/distance
                attenuation=(1-distance/light['radius'])**2/(1+.12*distance*distance);incident=np.asarray(color)*intensity*attenuation
                # Literal source default-mode compression and wrap, independent of GLSL.
                denominator=max(max(incident)*.75,1);incident*=1-compression+compression/denominator
                incident*=.25+np.clip(cosine+.5,0,1)*.75
                if name=='two-lights':
                    fill=np.asarray(color)*.25*attenuation
                    fill/=max(max(fill)*.75,1);fill*=.25+np.clip(cosine+.5,0,1)*.75;incident+=fill
                response=reference(linear([90,80,70]),linear([60,50,40]),np.array([180,150,170,153])/255,204/255,.55)
                expected=response*incident
                if name=='no-receiver':expected[:]=0
                if name in ('side','back','upper-light','lower-light'):assert max(expected)>.015,'Positive control must exceed output quantization and comparison budget'
                error=float(np.abs(measured-expected).max())
            report['captures'].append({'name':name,'maximumLinearError':error,'measured':measured.tolist(),
                'expected':None if expected is None else expected.tolist(),'imageSha256':hashlib.sha256(frame.read_bytes()).hexdigest(),'validationEnabled':validation})
            if error is not None:assert error<=.008,(name,error,measured,expected)
        for a,b in (('front','fixed'),('front','disabled-scene-npr'),('pbr','pbr-control')):
            if a in frames and b in frames:assert np.array_equal(frames[a],frames[b]),(a,b)
        report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path)
    p.add_argument('--case',action='append');p.add_argument('--expected-validation',choices=('enabled','disabled'));args=p.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.case,args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='character-local-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(args.executable,Path(temporary)/'capture',args.case,args.expected_validation)
