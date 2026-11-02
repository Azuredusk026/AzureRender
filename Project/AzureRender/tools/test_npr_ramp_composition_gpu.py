"""Measure view-ramp color composition through ordinary scene rendering."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import uuid
import numpy as np
from PIL import Image
from native_render.light_lab import generate
from create_visibility_fixture import component, surface, pad_material_profile_index
from convert_material_semantics import complete_material_document

BUDGET=.008
LUMA=np.array([.21267290413379669,.7151522040367126,.07217500358819962])
BASE=np.array([200,140,90,153],dtype=np.uint8)
RAMP=np.array([180,150,170,64],dtype=np.uint8)
def linear(value):
    v=np.asarray(value,dtype=float)/255
    return np.where(v<=.04045,v/12.92,((v+.055)/1.055)**2.4)
def write(path,value):path.write_bytes((json.dumps(value,indent=2)+'\n').encode())
def reference(policy,visibility,blend=False):
    lit=linear(BASE[:3])*.95999997854232788;shadow=lit*.275
    alpha=RAMP[3]/255;ao=BASE[3]/255;litMask=min(ao,alpha)
    saturation=lambda c:np.dot(c,LUMA)+(c-np.dot(c,LUMA))*1.2000000476837158
    weight=ao*alpha if policy=='view' else ao
    mix=np.clip(weight+alpha,0,1)
    shade=saturation(.64999997615814209*shadow)*(1-mix)+shadow*mix
    untinted=shade*(1-litMask)+lit*litMask;ramp=RAMP[:3]/255
    chroma=ramp.max()-ramp.min();tinted=untinted*(1-chroma+ramp*chroma)
    outdoor=tinted*np.clip(np.dot(untinted,LUMA)/max(np.dot(tinted,LUMA),.001),0,1.5)
    indoor=shadow*(1-weight)+saturation(lit)*weight
    color=indoor*(1-visibility)+outdoor*visibility
    return color*(BASE[3]/255 if blend else 1)

def prepare(folder,kind,policy,visibility,mode):
    generate(folder,'directional-near');path=folder/'plane.gltf';model=json.loads(path.read_text())
    Image.new('RGBA',(1,1),tuple(map(int,BASE))).save(folder/'base.png')
    Image.new('RGBA',(2,1),tuple(map(int,RAMP))).save(folder/'ramp.png')
    model.update(images=[{'uri':name+'.png'} for name in ['base','ramp']],textures=[{'source':i} for i in range(2)])
    material=model['materials'][0];material['pbrMetallicRoughness'].update(baseColorTexture={'index':0},baseColorFactor=[1,1,1,1],metallicFactor=0)
    material.update(alphaMode='BLEND' if mode=='blend' else 'MASK' if mode=='mask' else 'OPAQUE',alphaCutoff=.5)
    profile=dict(schemaVersion=2,**{'class':kind},features=['diffuse-ramp'],diffuseRamp=dict(schemaVersion=1,texture=1),styleParameters=[1,1,0,0],
        nprDiffuse=dict(schemaVersion=1,shadowScale=.275,shadowAlbedo='base-color',lightingInputs='host-scene-radiance',
            shadowMask='one',baseAo='albedo-alpha',characterVisibility='one'))
    if policy!='default':profile['nprDiffuse']['rampComposition']='view-ramp-alpha' if policy=='view' else 'mask-visibility'
    material['extras']=dict(azureSlotId='surface',azureRenderMaterial=profile)
    levelPath=folder/'level.azurelevel';level=json.loads(levelPath.read_text())
    level['renderSettings'].update(exposure=0,saturation=1,contrast=1,nprLighting=dict(schemaVersion=1,enabled=True,
        ambientColor=[1,1,1],ambientDirection=[0,1,0],directionBias=0,directionGain=0,directionFloor=1,
        outdoorGain=1,shadowAmbientGain=1,indoorMix=0,litAmbientMix=0,mainColorMix=0,shadowOverride=visibility,
        useSceneIrradiance=False))
    surface(folder/'blocker.gltf');level['resources'].append(dict(id='blocker',asset='assets:/blocker.gltf'))
    level['nodes'].append(dict(id='blocker',resourceId='blocker',components={'azure.transform':component('azure.transform',dict(translation=[1,0,1],scale=[.6,.6,.6]))}))
    level['lights'][0]['shape']['direction']=[-2**-.5,0,-2**-.5]
    if mode=='blend':
        surface(folder/'black-background.gltf')
        background=json.loads((folder/'black-background.gltf').read_text())
        background['materials'][0]['pbrMetallicRoughness']['baseColorFactor']=[0,0,0,1]
        write(folder/'black-background.gltf',background)
        level['resources'].append(dict(id='black-background',asset='assets:/black-background.gltf'))
        level['nodes'].append(dict(id='black-background',resourceId='black-background',components={
            'azure.transform':component('azure.transform',dict(translation=[0,0,-.1],scale=[8,8,1]))}))
    if mode=='document':
        identities={str(i):str(uuid.uuid4()) for i in range(2)}
        for i,name in enumerate(['base','ramp']):write(folder/(name+'.png.azmeta'),dict(schemaVersion=1,id=identities[str(i)],settings={},dependencies=[]))
        identity=str(uuid.uuid4());write(folder/'surface.azurematerial',complete_material_document(material,model,identities))
        write(folder/'surface.azurematerial.azmeta',dict(schemaVersion=1,id=identity,settings={},dependencies=list(identities.values())))
        next(n for n in level['nodes'] if n.get('resourceId')=='plane')['materialSlots']={'surface':identity}
        profile['nprDiffuse'].pop('rampComposition',None)
    pad_material_profile_index(model);write(path,model);write(levelPath,level)
    return folder/'lab.azureproject'

def run(player,output,selected=None,backends=('indexed','fixed'),expected_validation='any'):
    output.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    definitions=[(kind,policy,visibility,'opaque') for kind in ['generic','fabric'] for policy in ['default','mask','view'] for visibility in [0,.5,1]]
    definitions += [(kind,'view',.5,mode) for kind in ['generic','fabric'] for mode in ['mask','blend','msaa','document']]
    name=lambda c:c[0]+'-'+c[1]+'-'+str(c[2])+'-'+c[3]
    if selected:
        assert set(selected)<={name(c) for c in definitions};definitions=[c for c in definitions if name(c) in selected]
    override=os.environ.get('AZURERENDER_RESOURCE_ROOT')
    shaderDirectory=Path(override)/'shaders' if override and (Path(override)/'shaders').is_dir() else player.parent/'shaders'
    report=dict(status='running',budget=BUDGET,playerSha256=sha(player),shaderDirectory=str(shaderDirectory),shaders={p.name:sha(p) for p in shaderDirectory.glob('*.spv')},cases=[])
    images={};env=os.environ.copy();env.setdefault('DOTNET_ROOT','D:/Tools/dotnet9');startup=None
    if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    try:
        for backend in backends:
            for case in definitions:
                kind,policy,visibility,mode=case;label=backend+'-'+name(case);folder=output/label;print('START',label,flush=True)
                project=prepare(folder,kind,policy,visibility,mode)
                inputs={p.name:sha(p) for p in folder.iterdir() if p.is_file()}
                command=[str(player),'--project',str(project),'--width','128','--height','128','--capture-frames','1','--fixed-frame-step',
                    '--qa-camera','scene','--qa-light','scene','--qa-isolation','ambient','--capture-dir',str(folder/'frames'),'--runtime-report',str(folder/'runtime.json')]
                if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                if mode=='msaa':command+=['--anti-aliasing','multisample']
                row=dict(name=label,status='running',command=command,inputs=inputs);report['cases'].append(row)
                with (folder/'player.log').open('wb') as log:result=subprocess.run(command,cwd=folder,env=env,stdout=log,stderr=log,startupinfo=startup,timeout=150)
                row['exitCode']=result.returncode;assert result.returncode==0,label
                text=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text),label
                assert 'Allocator after unload: buffers=0 images=0' in text,label
                validation='Validation layer: enabled' in text
                if expected_validation!='any':assert validation==(expected_validation=='enabled'),label
                manifest=json.loads((folder/'frames/capture_manifest.json').read_text());assert manifest['materialSurfaceProfileLayoutVersion']==21 and manifest['capturedFrames']==1
                assert len(manifest['nprRampCompositions'])==(1 if policy=='view' else 0),label
                runtime=json.loads((folder/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors'],label
                image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'));images[label]=image
                expected=reference(policy,visibility,mode=='blend');actual=linear(image[60:68,60:68]);error=float(np.max(abs(actual-expected)))
                row.update(expected=expected.tolist(),actual=actual.mean(axis=(0,1)).tolist(),maximumLinearError=error,validationEnabled=validation,imageSha256=sha(folder/'frames/frame_000000.png'))
                assert error<BUDGET,(label,error,expected.tolist())
                for file,digest in inputs.items():assert sha(folder/file)==digest,file
                row['status']='passed';print('PASS',label,flush=True)
        for backend in backends:
            for kind in ['generic','fabric']:
                for visibility in [0,.5,1]:
                    a=backend+'-'+name((kind,'default',visibility,'opaque'));b=backend+'-'+name((kind,'mask',visibility,'opaque'))
                    if a in images and b in images:assert np.array_equal(images[a],images[b]),(a,b)
                a=backend+'-'+name((kind,'mask',0,'opaque'));b=backend+'-'+name((kind,'view',0,'opaque'))
                if a in images and b in images:assert np.max(abs(linear(images[a][64,64])-linear(images[b][64,64])))>.02,(kind,'positive view-ramp control')
        if 'indexed' in backends and 'fixed' in backends:
            for case in definitions:assert np.array_equal(images['indexed-'+name(case)],images['fixed-'+name(case)]),name(case)
        assert sha(player)==report['playerSha256'];report['status']='passed'
    finally:
        if report['status']=='running':report['status']='failed'
        write(output/'summary.json',report)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path)
    p.add_argument('--case',action='append');p.add_argument('--backend',action='append',choices=['indexed','fixed']);p.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');args=p.parse_args()
    engine=Path(__file__).resolve().parents[1];task=None
    if args.output is None:task=Path(subprocess.check_output([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--new-run','npr-ramp-composition'],text=True,cwd=engine).strip());args.output=task/'captures'
    try:run(args.executable.resolve(),args.output.resolve(),args.case,args.backend or ('indexed','fixed'),args.expected_validation)
    finally:
        if task:subprocess.run([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--finish',str(task)],cwd=engine,check=True)
