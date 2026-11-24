"""Check material radiance normalization against independent view exposure.

Fixtures exercise distinct lighting consumers. The eye uses its frozen oracle;
other comparisons use an unchanged material with a known -2 EV view transform.
No shader response formula is imported into the normalization oracle.
"""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import uuid

import numpy as np
from PIL import Image

from native_render.light_lab import generate
from create_visibility_fixture import pad_material_profile_index
from convert_material_semantics import complete_material_document
import test_projected_hair_gpu as hair_fixture

BUDGET=.008
ROOT=Path(__file__).resolve().parents[1]
FAMILIES=('unlit','pbr','npr','npr-document','eye','hair','transparent','multiply','outline','mixed')

def save(path,value):
    path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def linear(value):
    value=np.asarray(value,dtype=float)/255
    return np.where(value<=.04045,value/12.92,((value+.055)/1.055)**2.4)

def transform(translation,scale):
    return {'azure.transform':{'type':'azure.transform','version':1,'data':{'translation':translation,'scale':scale}}}

def prepare(folder,family,gain,exposure):
    oracle=None;pixels=[[32,64],[64,64],[96,64]];isolation='beauty'
    if family=='eye':
        source=ROOT/'tests/fixtures/eye_radial_surface/direct-linear-control'
        shutil.copytree(source,folder)
        model_path=folder/'assets/eye.gltf';level_path=folder/'assets/scene.azurelevel';project=folder/'test.azureproject'
        oracle=read(folder/'expected.json')['samples'];pixels=[x['pixel'] for x in oracle]
    elif family=='hair':
        case=next(x for x in hair_fixture.cases() if x['name']=='baseline')
        project,expected=hair_fixture.prepare(case,folder)
        model_path=folder/'assets/panels.gltf';level_path=folder/'assets/scene.azurelevel'
        pixels=[x['pixel'] for x in expected];isolation='hair-kk'
    else:
        generate(folder,'directional-near')
        model_path=folder/'plane.gltf';level_path=folder/'level.azurelevel';project=folder/'lab.azureproject'
    model=read(model_path);level=read(level_path)
    active=sorted({p.get('material',0) for m in model['meshes'] for p in m['primitives']})
    if family not in ('eye','hair'):
        material=model['materials'][active[0]];material['name']='radiance-subject'
        material['pbrMetallicRoughness']['baseColorFactor']=[.2,.3,.4,1]
        material['emissiveFactor']=[.1,.2,.3]
        profile={'schemaVersion':2,'class':'generic','features':['emissive-mask']}
        if family in ('unlit','transparent','multiply'):
            profile.update(features=['unlit-surface','emissive-mask'],surface={'schemaVersion':1,'lighting':'unlit'})
        if family=='transparent':
            material['alphaMode']='BLEND';material['pbrMetallicRoughness']['baseColorFactor'][3]=.5
            black=copy.deepcopy(model);black_material=black['materials'][active[0]]
            black_material['pbrMetallicRoughness']={'baseColorFactor':[0,0,0,1],'metallicFactor':0,'roughnessFactor':1}
            black_material.pop('emissiveFactor',None);black_material.pop('alphaMode',None)
            black_material['extras']={'azureRenderMaterial':{'schemaVersion':2,'class':'generic','features':['unlit-surface'],
                'surface':{'schemaVersion':1,'lighting':'unlit'}}}
            save(folder/'black.gltf',black)
            level['resources'].append({'id':'black','asset':'assets:/black.gltf'})
            level['nodes'].append({'id':'black','resourceId':'black','components':transform([0,0,-.2],[1,1,1])})
        if family in ('npr','npr-document','mixed'):
            for name,size,color in [('base',(1,1),(90,80,70,204)),('lut',(4,2),(60,50,40,255)),('ramp',(2,1),(180,150,170,153))]:
                Image.new('RGBA',size,color).save(folder/(name+'.png'))
            model.update(images=[{'uri':name+'.png'} for name in ('base','lut','ramp')],textures=[{'source':i} for i in range(3)])
            material['pbrMetallicRoughness'].update(baseColorFactor=[1,1,1,1],baseColorTexture={'index':0})
            profile.update({'class':'skin','features':['diffuse-ramp','emissive-mask'],
                'shadowColorLut':{'schemaVersion':1,'texture':1,'flipV':False},'diffuseRamp':{'schemaVersion':1,'texture':2},
                'styleParameters':[1,1,0,0],'nprDiffuse':{'schemaVersion':1,'shadowScale':.55,'lightingInputs':'host-scene-radiance','shadowMask':'none','baseAo':'albedo-alpha'}})
            level['renderSettings']['nprLighting']={'schemaVersion':1,'enabled':True,'ambientColor':[.3,.2,.1],'ambientDirection':[0,1,0],
                'directionBias':.15,'directionGain':1.5,'directionFloor':.5,'outdoorGain':1.05,'shadowAmbientGain':.8,
                'indoorMix':0,'litAmbientMix':1,'mainColorMix':1,'shadowOverride':1,'useSceneIrradiance':False}
        material.setdefault('extras',{})['azureRenderMaterial']=profile
        if family=='outline':
            profile['features'].append('silhouette-outline')
            profile['outlineSurface']={'schemaVersion':1,'mode':'view-scaled','width':.5,'minimumPixels':3,'color':[.12,.16,.2]}
            level['nodes'][0]['components']=transform([0,0,0],[.5,.5,1])
            accessor=model['accessors'][1];view=model['bufferViews'][accessor['bufferView']]
            raw=bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',',1)[1]))
            for i,(x,y) in enumerate([(-1,-1),(1,-1),(1,1),(-1,1)]):
                normal=np.array([x,y,.1]);normal/=np.linalg.norm(normal)
                struct.pack_into('<3f',raw,view.get('byteOffset',0)+i*12,*normal)
            model['buffers'][0]['uri']='data:application/octet-stream;base64,'+base64.b64encode(raw).decode()
        if family in ('mixed','multiply'):
            guard=copy.deepcopy(model)
            guard_material=guard['materials'][active[0]]
            guard_material['name']='radiance-control'
            guard_material.pop('emissiveFactor',None)
            guard_material['pbrMetallicRoughness']={'baseColorFactor':[.4,.3,.2,1],'metallicFactor':0,'roughnessFactor':1}
            guard_material['extras']={'azureRenderMaterial':{'schemaVersion':2,'class':'generic','features':[]}}
            node={'id':'guard','resourceId':'guard'}
            if family=='mixed':
                level['nodes'][0]['components']=transform([-2,0,0],[.5,1,1]);node['components']=transform([2,0,0],[.5,1,1]);pixels=[[24,64],[32,64],[40,64]]
            else:
                guard_material['alphaMode']='BLEND'
                guard_material['pbrMetallicRoughness']['baseColorFactor']=[.5,.6,.7,.5]
                guard_material['extras']['azureRenderMaterial']={'schemaVersion':2,'class':'overlay','features':['overlay','multiply-composite'],
                    'composite':{'schemaVersion':1,'mode':'multiply'},'radianceScale':1} if gain is not None else {'schemaVersion':2,'class':'overlay','features':['overlay','multiply-composite'],'composite':{'schemaVersion':1,'mode':'multiply'}}
                node['components']=transform([0,0,.1],[1,1,1])
            pad_material_profile_index(guard);save(folder/'guard.gltf',guard)
            level['resources'].append({'id':'guard','asset':'assets:/guard.gltf'});level['nodes'].append(node)
    for index in active:
        if gain is not None:model['materials'][index]['extras']['azureRenderMaterial']['radianceScale']=gain
    level['renderSettings'].update(exposure=exposure,toneMapping=False,antiAliasing=0,bloomEnabled=False,contrast=1,saturation=1)
    level['renderSettings']['gradeLut']={'schemaVersion':1,'asset':'','exposureMultiplier':1}
    if family=='npr-document':
        material=model['materials'][active[0]];material['extras']['azureSlotId']='subject'
        identities={str(i):str(uuid.uuid5(uuid.NAMESPACE_URL,str(folder)+':image:'+str(i))) for i in range(3)}
        for i,name in enumerate(('base','lut','ramp')):save(folder/(name+'.png.azmeta'),{'schemaVersion':1,'id':identities[str(i)],'settings':{},'dependencies':[]})
        document=complete_material_document(material,model,identities)
        identity=str(uuid.uuid5(uuid.NAMESPACE_URL,str(folder)+':material'))
        save(folder/'subject.azurematerial',document);save(folder/'subject.azurematerial.azmeta',{'schemaVersion':1,'id':identity,'settings':{},'dependencies':list(identities.values())})
        level['nodes'][0]['materialSlots']={'subject':identity}
    pad_material_profile_index(model);save(model_path,model);save(level_path,level)
    return {'project':project,'pixels':pixels,'oracle':oracle,'isolation':isolation}

def run(player,output,selected=(),expected_validation='any'):
    output.mkdir(parents=True,exist_ok=False);program_sha=hashlib.sha256(player.read_bytes()).hexdigest()
    shader_hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((player.parent/'shaders').glob('*.spv'))}
    assert shader_hashes,'Shader fingerprints are required'
    cases=[(f+'-'+name,f,gain,ev,False) for f in FAMILIES for name,gain,ev in [('control',None,0),('exposure',None,-2),('quarter',.25,0)]]
    cases += [('unlit-zero','unlit',0,0,False),('unlit-neutral','unlit',1,0,False)]
    cases += [(f+'-quarter-fixed',f,.25,0,True) for f in ('npr','npr-document','eye','hair')]
    if selected:
        assert set(selected)<={x[0] for x in cases};cases=[x for x in cases if x[0] in selected]
    summary={'status':'running','programSha256':program_sha,'shaderSha256':shader_hashes,'maximumLinearErrorBudget':BUDGET,'nativeEquivalenceClaimed':False,'captures':[],'comparisons':[]};images={};definitions={}
    try:
        for name,family,gain,ev,fixed in cases:
            folder=output/name;definition=prepare(folder,family,gain,ev);definitions[name]=definition
            command=[str(player),'--project',str(definition['project']),'--width','128','--height','128','--capture-dir',str(folder/'frames'),
                '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation',definition['isolation'],'--runtime-report',str(folder/'runtime.json')]
            if family=='hair':command+=['--qa-exposure',str(ev)]
            if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            save(folder/'command.json',command);result=subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=120,cwd=folder)
            log=result.stdout+result.stderr;(folder/'player.log').write_text(log,encoding='utf-8')
            row={'name':name,'command':command,'exitCode':result.returncode};summary['captures'].append(row)
            assert result.returncode==0,(name,result.returncode,log[-1000:])
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I),name
            diagnostics=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text(encoding='utf-8').splitlines()]
            assert not any(item['level']=='error' for item in diagnostics),name
            assert any(item['message']=='Allocator after unload: buffers=0 images=0' for item in diagnostics),name
            row['allocatorUnloadVerified']=True
            row['allocatorUnloadOnStdout']='Allocator after unload: buffers=0 images=0' in log
            if expected_validation!='any':assert ('Validation layer: enabled' in log)==(expected_validation=='enabled'),name
            image=np.array(Image.open(folder/'frames/frame_000000.png').convert('RGB'));images[name]=image
            manifest=read(folder/'frames/capture_manifest.json');assert manifest['capturedFrames']==1 and manifest['materialSurfaceProfileLayoutVersion']==19,name
            overrides=manifest['materialRadianceScales']
            if gain is not None and gain!=1:assert overrides and all(x['radianceScale']==gain for x in overrides),(name,overrides)
            else:assert overrides==[],(name,overrides)
            runtime=read(folder/'runtime.json');assert runtime['scriptErrors']==[] and runtime['presentationErrors']==[],name
            row['imageSha256']=hashlib.sha256((folder/'frames/frame_000000.png').read_bytes()).hexdigest()
            if definition['oracle']:
                wanted=np.array([x['linearRgb'] for x in definition['oracle']])*(1 if gain is None else gain)*(2**ev)
                actual=np.array([linear(image[y,x]) for x,y in definition['pixels']]);error=float(np.abs(actual-wanted).max())
                row['independentEyeOracleError']=error;assert error<=BUDGET,(name,error)
            assert hashlib.sha256(player.read_bytes()).hexdigest()==program_sha,'Program changed during validation'
            print('CAPTURED',name,flush=True)
        for family in FAMILIES:
            ref,scaled=family+'-exposure',family+'-quarter'
            if ref not in images or scaled not in images:continue
            if family=='outline':
                control=images[family+'-control'];mask=np.any(control!=control[0,0],axis=2);assert mask.sum()>128,'Outline fixture requires a visible surface and shell'
                a=linear(images[ref][mask]);b=linear(images[scaled][mask])
            else:
                pixels=definitions[scaled]['pixels'];a=np.array([linear(images[ref][y,x]) for x,y in pixels]);b=np.array([linear(images[scaled][y,x]) for x,y in pixels])
            error=float(np.abs(a-b).max());assert np.max(a)>.008,(family,'Missing positive radiance control')
            summary['comparisons'].append({'family':family,'maximumLinearError':error});assert error<=BUDGET,(family,error)
        if all(x in images for x in ['mixed-control','mixed-quarter']):
            a=images['mixed-control'][16:112,80:112];b=images['mixed-quarter'][16:112,80:112]
            assert np.array_equal(a,b),'Unscaled ordinary surface changed';summary['ordinarySurfaceGuardExact']=True
        if 'unlit-zero' in images:assert np.max(images['unlit-zero'][32:96,32:96])==0,'Zero material radiance must be black'
        if all(x in images for x in ['unlit-control','unlit-neutral']):assert np.array_equal(images['unlit-control'],images['unlit-neutral']),'Neutral radiance must retain exact pixels'
        for family in ('npr','npr-document','eye','hair'):
            a,b=family+'-quarter',family+'-quarter-fixed'
            if a in images and b in images:assert np.array_equal(images[a],images[b]),(family,'Descriptor paths differ')
        if all(x in images for x in ['npr-quarter','npr-document-quarter']):
            error=float(np.abs(linear(images['npr-quarter'])-linear(images['npr-document-quarter'])).max())
            summary['documentInlineMaximumLinearError']=error;assert error<=BUDGET,('Inline/document inputs differ',error)
        assert shader_hashes=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((player.parent/'shaders').glob('*.spv'))},'Shaders changed during validation'
        summary['status']='passed';print(json.dumps({'status':'passed','captures':len(images),'comparisons':len(summary['comparisons'])}))
    except Exception as error:
        summary.update(status='failed',failure=str(error));raise
    finally:save(output/'summary.json',summary)

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--case',action='append',default=[]);parser.add_argument('--expected-validation',choices=('any','enabled','disabled'),default='any');args=parser.parse_args()
    owned=None
    if args.output is None:
        owned=Path(subprocess.check_output([sys.executable,str(ROOT/'tools/artifact_cleanup.py'),'--new-run','material-radiance-gpu'],text=True,cwd=ROOT).strip())
        args.output=owned/'capture'
    try:run(args.executable.resolve(),args.output.resolve(),args.case,args.expected_validation)
    finally:
        if owned is not None:subprocess.run([sys.executable,str(ROOT/'tools/artifact_cleanup.py'),'--finish',str(owned)],check=True,cwd=ROOT)
