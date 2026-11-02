"""Measure material environment sources and character-main object shadows."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import uuid

import numpy as np
from PIL import Image
from test_character_probe_gpu import prepare as probe_fixture, linear
from native_render.light_lab import generate
from native_render.probe_field_lab import prepare as probe_field_fixture
from convert_material_semantics import complete_material_document

GLOBAL=np.array([.12,.24,.36]);PROBE=np.array([.8,.4,.2]);GI=np.array([.8,.2,.4]);BASE=np.array([.4,.3,.2]);BUDGET=.008

def write(path,value):path.write_bytes((json.dumps(value,indent=2)+'\n').encode())

def environment_fixture(folder,kind,mode):
    project=probe_fixture(folder,dict(name=mode,material=kind));assets=folder/'assets';path=assets/'surface.gltf';model=json.loads(path.read_text());material=model['materials'][0];material['pbrMetallicRoughness'].update(baseColorFactor=[*BASE,1]);profile=material['extras']['azureRenderMaterial'];profile['styleParameters']=[0,0,1,0]
    for name,rgb in [('global',GLOBAL),('probe',PROBE)]:
        (assets/(name+'.azenv')).write_bytes(b'AZENV001'+struct.pack('<III',2,1,1)+struct.pack('<4e',*rgb,1)*2)
    cell=struct.pack('<16f',0,0,0,GI[0],0,0,0,GI[1],0,0,0,GI[2],1,0,0,0)
    (assets/'field.aziv').write_bytes(struct.pack('<8s4f3I',b'AZIV0001',-4,-4,-4,8,2,2,2)+cell*8)
    path_level=assets/'scene.azurelevel';level=json.loads(path_level.read_text());level['lights']=[];level['renderSettings'].update(schemaVersion=20,skyIntensity=.75 if mode.endswith('inherit') else 1,environmentSpecularModel='legacy-schlick');level['irradianceVolume']=dict(version=1,asset='assets:/field.aziv',enabled=True,intensity=1)
    isolation='ambient' if mode.startswith('diffuse') else 'reflection' if mode.startswith('reflection') else 'beauty';global_diffuse=mode.startswith('diffuse') and not mode.endswith(('default','scene')) or mode in ['composed','mix-diffuse'];global_reflection=mode.startswith('reflection') and not mode.endswith(('default','scene')) or mode in ['composed','mix-reflection'];intensity=.5 if mode.endswith('half') else 0 if mode.endswith('zero') else .75 if mode.endswith('inherit') else 1
    if not mode.endswith('default'):
        policy=dict(schemaVersion=1,diffuse='global-environment' if global_diffuse else 'scene',reflection='global-environment' if global_reflection else 'scene')
        if (global_diffuse or global_reflection) and not mode.endswith('inherit'):policy['globalIntensity']=intensity
        profile['environmentLighting']=policy
    ambient=BASE*(GLOBAL*intensity if global_diffuse else GI);reflection=.08*(GLOBAL*intensity if global_reflection else PROBE);expected=ambient if isolation=='ambient' else reflection if isolation=='reflection' else ambient+reflection
    # A nonzero profile index checks the common packed stride independently of scene order.
    model['materials'].insert(0,dict(name='padding',pbrMetallicRoughness=dict(baseColorFactor=[0,0,0,1])))
    for mesh in model['meshes']:
        for primitive in mesh['primitives']:primitive['material']=primitive.get('material',0)+1
    write(path,model);write(path_level,level);return project,['--environment',str(assets/'global.azenv'),'--qa-isolation',isolation],expected

def object_fixture(folder,kind,mode):
    generate(folder,'directional-near');path=folder/'plane.gltf';model=json.loads(path.read_text());material=model['materials'][0];material['doubleSided']=True;material.setdefault('extras',{})['azureSlotId']='surface';material['pbrMetallicRoughness'].update(baseColorFactor=[*BASE,1],metallicFactor=0,roughnessFactor=1);profile=dict(schemaVersion=2,**{'class':kind},features=[],styleParameters=[0,1,0,0])
    if mode not in ['legacy','reference']:profile['mainLightShadow']='object-shadow'
    material['extras']['azureRenderMaterial']=profile
    if mode not in ['clear','reference']:model['nodes'].append(dict(mesh=0,translation=[0,0,1],scale=[.125,.125,1]));model['scenes'][0]['nodes'].append(1)
    write(path,model);level=json.loads((folder/'level.azurelevel').read_text());level['renderSettings'].update(sceneLighting=True,skyIntensity=0,shadowDistance=20);level['lights'][0]['shape'].update(direction=[.6,0,-.8],receivers=dict(schemaVersion=1,surfaces=True,characters=True))
    other=copy.deepcopy(level['lights'][0]);other.update(id='surface-main',intensity=3,color=[0,1,0]);other['shape'].update(direction=[0,0,-1],receivers=dict(schemaVersion=1,surfaces=True,characters=False));level['lights'].append(other)
    if mode=='reference':level['lights']=level['lights'][:1]
    receiver=next(n for n in level['nodes'] if n.get('resourceId')=='plane');receiver['components']={'azure.object-shadow':dict(type='azure.object-shadow',version=1,data=dict(enabled=mode!='disabled',center=[0,0,.5],halfExtents=[6,6,3],normalBias=.1,lightBias=.05,filterRadiusX=4,filterRadiusY=2,atlasPage=6))}
    if mode=='reference':receiver['components']={}
    if mode=='document':
        identity=str(uuid.uuid4());write(folder/'surface.azurematerial',complete_material_document(material,model,{}));write(folder/'surface.azurematerial.azmeta',dict(schemaVersion=1,id=identity,settings={},dependencies=[]));receiver['materialSlots']={'surface':identity}
        material['extras']['azureRenderMaterial'].pop('mainLightShadow');write(path,model)
    # Ordinary direct diffuse retains the established 0.62 response gain.
    # The unchanged default receiver below is an independent image control.
    write(folder/'level.azurelevel',level);return folder/'lab.azureproject',['--qa-isolation','direct-diffuse'],BASE*.3*.8*.62

def field_fixture(folder,kind,mode):
    project=probe_field_fixture(folder/'project',dict(name='adapted'));path=folder/'project/assets/surface.gltf';model=json.loads(path.read_text());profile=dict(schemaVersion=2,**{'class':kind},features=[],styleParameters=[0,0,1,0])
    if mode=='field-diffuse-global':profile['environmentLighting']=dict(schemaVersion=1,diffuse='global-environment')
    model['materials'][0]['extras']={'azureRenderMaterial':profile};write(path,model)
    return project,['--qa-isolation','reflection'],np.array([.25,.0625,.125])

def run(player,output,selected=None,backends=('indexed','fixed'),expected_validation='any'):
    output.mkdir(parents=True,exist_ok=False);modes=['diffuse-default','diffuse-scene','diffuse-global','diffuse-half','diffuse-zero','diffuse-inherit','reflection-default','reflection-scene','reflection-global','reflection-half','composed','mix-diffuse','mix-reflection','field-default','field-diffuse-global'];definitions=[(kind,mode) for kind in ['generic','fabric'] for mode in modes+['object-'+m for m in ['reference','clear','shadow','disabled','legacy','document']]];names={kind+'-'+mode for kind,mode in definitions}
    if selected:assert set(selected)<=names;definitions=[(k,m) for k,m in definitions if k+'-'+m in selected]
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();frozen=dict(playerSha256=sha(player),scriptSha256=sha(Path(__file__)),shaders={p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')});report=dict(status='running',linearBudget=BUDGET,cases=[],**frozen);images={};env=os.environ.copy();env.setdefault('DOTNET_ROOT','D:/Tools/dotnet9');startup=None
    if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    try:
        for backend in backends:
            for kind,mode in definitions:
                name=backend+'-'+kind+'-'+mode;folder=output/name;is_object=mode.startswith('object-')
                if not is_object:folder.mkdir()
                project,extra,expected=object_fixture(folder,kind,mode[7:]) if is_object else field_fixture(folder,kind,mode) if mode.startswith('field-') else environment_fixture(folder,kind,mode);write(folder/'expected.json',dict(linearColor=expected.tolist(),budget=BUDGET));inputs={p.relative_to(folder).as_posix():sha(p) for p in folder.rglob('*') if p.is_file()};width=256 if is_object else 128;command=[str(player),'--project',str(project),'--width',str(width),'--height',str(width),'--capture-dir',str(folder/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--runtime-report',str(folder/'runtime.json'),*extra]
                if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                row=dict(name=name,status='running',command=command,inputs=inputs);report['cases'].append(row);print('START',name,flush=True)
                with (folder/'player.log').open('wb') as log:result=subprocess.run(command,cwd=folder,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=120)
                row['exitCode']=result.returncode;assert result.returncode==0,(name,'Player failure');logs=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()];assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in logs);assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in logs);assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(folder/'player.log').read_text(errors='replace'),re.I)
                manifest=json.loads((folder/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==1 and manifest['materialSurfaceProfileLayoutVersion']==20;runtime=json.loads((folder/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors'];image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'));images[name]=image
                validation='Validation layer: enabled' in (folder/'player.log').read_text(errors='replace');row['validationEnabled']=validation
                if expected_validation!='any':assert validation==(expected_validation=='enabled'),name
                if is_object:
                    control=linear(image[124:132,84:92]);shadow=linear(image[126:130,150:154]);row.update(controlLinear=control.mean(axis=(0,1)).tolist(),shadowLinear=shadow.mean(axis=(0,1)).tolist())
                    if mode!='object-legacy':
                        assert np.max(np.abs(control-expected))<=BUDGET,(name,'clear receiver radiance');assert np.max(shadow)<=BUDGET if mode in ['object-shadow','object-document'] else np.max(np.abs(shadow-expected))<=BUDGET,(name,'shadow reception')
                    else:assert control.max()>.01 and shadow.max()>.01,(name,'legacy positive control')
                else:
                    error=float(np.max(np.abs(linear(image[62:66,62:66])-expected)));row['maximumLinearError']=error;assert error<=BUDGET,(name,error,expected)
                for path,digest in inputs.items():assert sha(folder/path)==digest,path
                row.update(status='passed',imageSha256=sha(folder/'frames/frame_000000.png'));print('PASS',name,flush=True)
        for backend in backends:
            for kind in ['generic','fabric']:
                for left,right in [('diffuse-default','diffuse-scene'),('reflection-default','reflection-scene'),('field-default','field-diffuse-global'),('object-clear','object-reference'),('object-shadow','object-document')]:
                    a=backend+'-'+kind+'-'+left;b=backend+'-'+kind+'-'+right
                    if a in images and b in images:assert np.array_equal(images[a],images[b]),(a,b)
        if 'indexed' in backends and 'fixed' in backends:
            for kind,mode in definitions:assert np.array_equal(images['indexed-'+kind+'-'+mode],images['fixed-'+kind+'-'+mode]),(kind,mode,'descriptor paths differ')
        assert sha(player)==frozen['playerSha256'] and {p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')}==frozen['shaders'];report['status']='passed'
    finally:
        if report['status']=='running':report['status']='failed'
        write(output/'summary.json',report)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--player','--executable',dest='player',type=Path,required=True);parser.add_argument('--output',type=Path);parser.add_argument('--case',action='append');parser.add_argument('--backend',action='append',choices=['indexed','fixed']);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');args=parser.parse_args();engine=Path(__file__).resolve().parents[1];task=None
    if args.output is None:task=Path(subprocess.check_output([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--new-run','material-lighting-gpu'],cwd=engine,text=True).strip());args.output=task/'validation'
    try:run(args.player.resolve(),args.output.resolve(),args.case,args.backend or ('indexed','fixed'),args.expected_validation)
    finally:
        if task is not None:subprocess.run([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--finish',str(task)],cwd=engine,check=True)
