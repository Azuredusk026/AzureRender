"""Exercise character rim sampling against the production linear-depth prepass."""
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
    names=('edge','interior','near-depth','mid-depth','far-depth','zero-offset','zero-tint','spot','fixed',
        'disabled','effect-disabled','no-receiver','pbr','pbr-control','pbr-specular','parallel','blend','rim-isolation','smooth')
    names+=('material-depth','material-facing','material-facing-fixed','material-facing-near-depth','material-facing-far-depth',
        'material-facing-front','material-facing-disabled','material-facing-effect-disabled','material-facing-no-receiver','material-facing-generic','pbr-material','pbr-material-control')
    if selected:
        assert set(selected)<=set(names);names=tuple(n for n in names if n in selected)
    report=dict(schemaVersion=1,status='running',playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(),captures=[]);frames={}
    try:
        for name in names:
            print(name,flush=True);folder=output/name;generate(folder,'dark')
            material_rim=name.startswith('material-') or name.startswith('pbr-material')
            view_facing=name.startswith('material-facing')
            model_path=folder/'plane.gltf';model=json.loads(model_path.read_text());mat=model['materials'][0]
            for key,color,size in [('ramp',(255,255,255,255),(2,1)),('lut',(128,128,128,255),(4,2)),
                ('normal',(204,128,230,255),(1,1))]:Image.new('RGBA',size,color).save(folder/(key+'.png'))
            model.update(images=[dict(uri=p+'.png') for p in ('ramp','lut','normal')],textures=[dict(source=i) for i in range(3)])
            mat['pbrMetallicRoughness'].update(baseColorFactor=[.25,.25,.25,1],metallicFactor=0,roughnessFactor=1)
            mat['normalTexture']=dict(index=2)
            mat['extras']={'azureRenderMaterial':{'schemaVersion':2,'class':'skin','features':['diffuse-ramp'],
                'diffuseRamp':dict(schemaVersion=1,texture=0),'shadowColorLut':dict(schemaVersion=1,texture=1,flipV=False),
                'styleParameters':[1,1,0,0],'nprDiffuse':dict(schemaVersion=1,shadowScale=1,lightingInputs='host-scene-radiance',shadowMask='none',baseAo='one')}}
            if name.startswith('pbr'):mat.pop('extras')
            if view_facing:mat['extras']['azureRenderMaterial']['nprDiffuse']['rimResponse']='view-facing'
            if name=='material-facing-generic':mat['extras']['azureRenderMaterial']['class']='generic'
            if name=='material-facing-front':Image.new('RGBA',(1,1),(128,128,255,255)).save(folder/'normal.png')
            model_path.write_text(json.dumps(model),encoding='utf-8')
            path=folder/'level.azurelevel';level=json.loads(path.read_text());level['nodes'][0]['components']={'azure.transform':dict(type='azure.transform',version=1,data=dict(scale=[.35,.35,.35]))}
            response=dict(schemaVersion=4,composition='screen-rim',offsetScale=0 if name=='zero-offset' else 16,surfaceTint=0 if name=='zero-tint' else .8)
            if material_rim:response=dict(schemaVersion=5,composition='material-rim',width=1 if view_facing else 16,surfaceTint=.8)
            if name=='pbr-specular':response=dict(schemaVersion=4,composition='specular-only')
            position=[1.28125,-.03125,1] if name=='parallel' else [2,0,1]
            level['nodes'][2]['components']['azure.transform']['data']['translation']=position
            shape=dict(type='spot' if name=='spot' else 'point',direction=[-2**-.5,0,-2**-.5],
                innerCone=.25,outerCone=.5,characterResponse=response,falloff='smooth-inverse-square' if name=='smooth' else 'legacy')
            if name in ('no-receiver','material-facing-no-receiver'):shape['receivers']=dict(schemaVersion=1,surfaces=True,characters=False)
            level['lights']=[dict(id='rim',nodeId='light',enabled=True,color=[.8,.6,.4],intensity=1.5,radius=10,shape=shape)]
            if name.startswith('pbr'):
                if name.endswith('-control'):level['lights']=[]
                physical=copy.deepcopy(level['lights'][0]) if level['lights'] else dict(nodeId='light',color=[.8,.6,.4],intensity=1.5,radius=10)
                physical.update(id='physical',enabled=True,shape=dict(type='point'));level['lights'].append(physical)
            depth_case=next((key for key in ('near-depth','mid-depth','far-depth') if name==key or name.endswith('-'+key)),None)
            if depth_case:
                z={'near-depth':-.05,'mid-depth':-.15,'far-depth':-.3}[depth_case];back=copy.deepcopy(model)
                back['materials'][0].pop('extras');back['materials'][0]['pbrMetallicRoughness']['baseColorFactor']=[0,0,0,1]
                (folder/'back.gltf').write_text(json.dumps(back),encoding='utf-8')
                level['resources'].append(dict(id='back',asset='assets:/back.gltf'))
                level['nodes'].append(dict(id='back',resourceId='back',components={'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[0,0,z]))}))
            if name=='blend':level['lights'].append(dict(id='blend',nodeId='light',enabled=True,color=[.1,.2,.3],intensity=1,radius=2,
                shape=dict(type='point',falloff='range-power',falloffExponent=2,characterResponse=dict(schemaVersion=3,composition='color-blend',strength=.7,facingWeight=0,projectedDistance=False,order=8))))
            level['renderSettings']['nprLighting']=dict(schemaVersion=1,enabled=name not in ('disabled','material-facing-disabled'),ambientColor=[0,0,0],ambientDirection=[0,1,0],directionBias=0,
                directionGain=0,directionFloor=1,outdoorGain=1,shadowAmbientGain=1,indoorMix=0,litAmbientMix=0,mainColorMix=0,shadowOverride=1,useSceneIrradiance=False)
            path.write_text(json.dumps(level),encoding='utf-8')
            command=[str(player.resolve()),'--project',str(folder/'lab.azureproject'),'--width','128','--height','128','--capture-dir',str(folder/'frames'),
                '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene']
            if name in ('fixed','material-facing-fixed'):command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            if name=='rim-isolation':command+=['--qa-isolation','rim']
            if name in ('effect-disabled','material-facing-effect-disabled'):command+=['--qa-effect','rim','--qa-effect-state','disabled']
            (folder/'command.json').write_text(json.dumps(command,indent=2),encoding='utf-8')
            with (folder/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log
            validation='Validation layer: enabled' in log
            if expected_validation:assert validation==(expected_validation=='enabled')
            if material_rim:
                manifest=json.loads((folder/'frames/capture_manifest.json').read_text())
                assert manifest['materialSurfaceProfileLayoutVersion']==5 and manifest['lightBufferLayoutVersion']==3
            frame=folder/'frames/frame_000000.png';pixels=np.asarray(Image.open(frame).convert('RGB'));frames[name]=pixels
            x=64 if name=='interior' else 84;measured=linear(pixels[64,x]);point=np.array([(x+.5)/128*8-4,-.03125,0])
            delta=np.asarray(position)-point;squared=np.dot(delta,delta);distance=np.sqrt(squared);light=delta/distance
            normal=np.array([128,128,255] if name=='material-facing-front' else [204,128,230])/255*2-1;normal/=np.linalg.norm(normal)
            planar=light.copy();planar[2]=0;length=np.linalg.norm(planar);planar=planar/length if length>1e-6 else np.zeros(3)
            attenuation=(1-distance/10)**2/(1+.12*squared)
            if name=='smooth':attenuation=max(1-(squared/100)**2,0)**2/(squared+1)
            depth_weight=0 if name in ('interior','zero-offset') else {'near-depth':0,'mid-depth':.5,'far-depth':1}.get(name,1)
            if view_facing:
                view=np.array([0,0,5])-point;view/=np.linalg.norm(view);width=response['width']
                band=np.clip(((1-abs(np.dot(normal,view)))-(.8-.6*width))/(.1+.2*width),0,1);depth_weight=band*band*(3-2*band)
            facing=np.clip(np.dot(normal,planar),0,1);tint=.5+response.get('surfaceTint',0)*(.25*.96-.5)
            expected=np.array([.8,.6,.4])*1.5*attenuation*depth_weight*facing*tint
            if name in ('disabled','effect-disabled','no-receiver','material-facing-disabled','material-facing-effect-disabled','material-facing-no-receiver'):expected[:]=0
            if name=='blend':
                weight=.7*max(1-min(squared/4,1),0)**2;expected=expected*(1-weight)+np.array([.1,.2,.3])*weight
            if name=='rim-isolation':expected*=3
            error=None if name.startswith('pbr') else float(np.abs(measured-expected).max())
            if error is None:assert max(measured)>.015,'Physical compatibility controls must remain visibly lit'
            else:assert error<=.008,(name,error,measured,expected)
            if name in ('edge','mid-depth','zero-tint','spot','smooth'):assert max(expected)>.015,'Edge control must exceed comparison tolerance'
            report['captures'].append(dict(name=name,maximumLinearError=error,measured=measured.tolist(),expected=None if error is None else expected.tolist(),
                imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest(),validationEnabled=validation))
        for a,b in (('edge','fixed'),('pbr','pbr-control'),('pbr-specular','pbr-control')):
            if a in frames and b in frames:assert np.array_equal(frames[a],frames[b]),(a,b)
        for a,b in (('edge','material-depth'),('material-facing','material-facing-fixed'),('material-facing','material-facing-generic'),('pbr-material','pbr-material-control')):
            if a in frames and b in frames:assert np.array_equal(frames[a],frames[b]),(a,b)
        if 'material-facing-near-depth' in frames and 'material-facing-far-depth' in frames:
            assert np.array_equal(frames['material-facing-near-depth'][64,84],frames['material-facing-far-depth'][64,84])
        report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path)
    p.add_argument('--case',action='append');p.add_argument('--expected-validation',choices=('enabled','disabled'));a=p.parse_args()
    if a.output:run(a.executable,a.output.resolve(),a.case,a.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='screen-rim-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temporary:
            run(a.executable,Path(temporary)/'capture',a.case,a.expected_validation)
