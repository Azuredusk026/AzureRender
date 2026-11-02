"""Validate material display policies with independent, ungraded Player controls."""
import argparse
import base64
import copy
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

import numpy as np
from PIL import Image

from test_material_radiance_gpu import prepare as prepare_radiance
from test_unlit_texture_layers_gpu import profile as layer_profile

ROOT=Path(__file__).resolve().parents[1]
BUDGET=.008
CONSTANT=np.array([.03,.05,.08])
FAMILIES=('unlit','pbr','npr','npr-document','eye','hair','transparent','multiply','outline','mixed',
          'cutout','opacity','layers','additive','multisample','temporal')

def save(path,value):
    raw=(json.dumps(value,indent=2)+'\n').encode('utf-8')
    if path.exists():
        with path.open('r+b') as output:output.write(raw);output.truncate()
    else:path.write_bytes(raw)

def linear(value):
    value=np.asarray(value,dtype=float)/255
    return np.where(value<=.04045,value/12.92,((value+.055)/1.055)**2.4)

def png_uri(color):
    output=io.BytesIO();Image.new('RGBA',(1,1),color).save(output,format='PNG')
    return 'data:image/png;base64,'+base64.b64encode(output.getvalue()).decode()

def constant_lut(path):
    header=dict(schemaVersion=1,dimension=2,inputDomain='linear',outputDomain='linear',layout='strip-rgb',
        precision='float32-le',channels=4,width=4,height=2,
        source=dict(generator='azure.public-display-policy.fixture',generatorVersion=1,
                    inputFingerprint='constant-rgb-003-005-008-v1',evidence=0))
    encoded=json.dumps(header,separators=(',',':')).encode()
    path.write_bytes(b'AZLUT001'+struct.pack('<I',len(encoded))+encoded+struct.pack('<32f',*([.03,.05,.08,1]*8)))

def prepare(folder,family,mode):
    base=family if family in FAMILIES[:10] else 'unlit'
    definition=prepare_radiance(folder,base,.25,0)
    assets=folder/'assets' if base in ('eye','hair') else folder
    model_path=assets/('eye.gltf' if base=='eye' else 'panels.gltf' if base=='hair' else 'plane.gltf')
    level_path=assets/('scene.azurelevel' if base in ('eye','hair') else 'level.azurelevel')
    model=json.loads(model_path.read_text());level=json.loads(level_path.read_text())
    active=sorted({primitive.get('material',0) for mesh in model['meshes'] for primitive in mesh['primitives']})
    if family=='layers':
        model['images']=[{'uri':png_uri((200,100,160,255))}];model['textures']=[{'source':0}]
        for index in active:
            material=model['materials'][index]
            material['alphaMode']='BLEND';material.pop('emissiveFactor',None)
            material['pbrMetallicRoughness']={'baseColorFactor':[1,1,1,1]}
            material['extras']['azureRenderMaterial']={'schemaVersion':2,'class':'generic',
                'features':['unlit-surface','unlit-texture-layers'],'radianceScale':.25,'surface':layer_profile()}
    if family=='additive':
        overlay=copy.deepcopy(model)
        overlay['images']=[{'uri':png_uri((100,60,160,128))}];overlay['textures']=[{'source':0}]
        for index in active:
            material=overlay['materials'][index];surface=layer_profile();surface['composite']='additive'
            material.update(alphaMode='BLEND',name='inherited-additive')
            material.pop('emissiveFactor',None)
            material['pbrMetallicRoughness']={'baseColorFactor':[1,1,1,1],'metallicFactor':0,'roughnessFactor':1}
            material['extras']['azureRenderMaterial']={'schemaVersion':2,'class':'generic',
                'features':['unlit-surface','unlit-texture-layers'],'surface':surface}
        save(folder/'additive.gltf',overlay);level['resources'].append({'id':'additive','asset':'assets:/additive.gltf'})
        node=copy.deepcopy(level['nodes'][0]);node.update(id='additive',resourceId='additive')
        node['components']={'azure.transform':{'type':'azure.transform','version':1,'data':{'translation':[0,0,.1]}}};level['nodes'].append(node)
    if family=='cutout':
        mask=Image.new('RGBA',(2,1));mask.putdata([(255,255,255,0),(255,255,255,255)]);mask.save(folder/'cutout.png')
        model['images']=[{'uri':'cutout.png'}];model['textures']=[{'source':0}]
        for index in active:
            material=model['materials'][index];material.update(alphaMode='MASK',alphaCutoff=.5)
            material['pbrMetallicRoughness']['baseColorTexture']={'index':0}
    if family=='opacity':
        model.setdefault('extensionsUsed',[]).append('AZURE_node_opacity')
        for node in model['nodes']:
            if 'mesh' in node:node.setdefault('extensions',{})['AZURE_node_opacity']={'schemaVersion':1,'opacity':.4}
    frames=32 if family=='temporal' else 1
    if family=='multisample':
        level['renderSettings']['schemaVersion']=19
        level['renderSettings']['antiAliasing']=4
        level['nodes'][0]['components']={'azure.transform':{'type':'azure.transform','version':1,
            'data':{'rotation':[0,0,17],'scale':[.65,.65,1]}}}
    elif family=='temporal':
        level['renderSettings'].update(schemaVersion=18,antiAliasing=3)
        level['nodes'][0]['components']={'azure.transform':{'type':'azure.transform','version':1,
            'data':{'rotation':[0,0,17],'scale':[.65,.65,1]}}}
    for index in active:
        profile=model['materials'][index]['extras']['azureRenderMaterial']
        if mode in ('linear','explicit-scene'):profile['displayTransform']='linear' if mode=='linear' else 'scene'
    if base=='npr-document' and mode=='linear':
        path=folder/'subject.azurematerial';document=json.loads(path.read_text())
        document['surface']['materials'][0]['extras']['azureRenderMaterial']['displayTransform']='linear';save(path,document)
    if mode!='reference':
        constant_lut(assets/'display.azlut')
        level['renderSettings']['gradeLut']={'schemaVersion':1,'asset':'assets:/display.azlut','exposureMultiplier':1}
    save(model_path,model);save(level_path,level)
    return dict(project=definition['project'],pixels=definition['pixels'],oracle=definition['oracle'],frames=frames)

def run(player,output,selected,expected_validation):
    output.mkdir(parents=True,exist_ok=False)
    digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    program=digest(player);shaders={p.name:digest(p) for p in sorted((player.parent/'shaders').glob('*.spv'))}
    cases=[(family+'-'+mode,family,mode,False) for family in FAMILIES for mode in ('reference','scene','linear')]
    cases += [(family+'-linear-fixed',family,'linear',True) for family in ('npr','npr-document','eye','hair')]
    cases += [('unlit-explicit-scene','unlit','explicit-scene',False)]
    if selected:
        assert set(selected)<={row[0] for row in cases};cases=[row for row in cases if row[0] in selected]
    report=dict(status='running',programSha256=program,shaderSha256=shaders,maximumLinearErrorBudget=BUDGET,
        visualAcceptanceClaimed=False,captures=[],comparisons=[])
    images={};definitions={}
    try:
        for name,family,mode,fixed in cases:
            folder=output/name;definition=prepare(folder,family,mode);definitions[name]=definition
            command=[str(player),'--project',str(definition['project']),'--width','128','--height','128',
                '--capture-frames',str(definition['frames']),'--fixed-frame-step','--capture-dir',str(folder/'frames'),
                '--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty','--runtime-report',str(folder/'runtime.json')]
            if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            save(folder/'command.json',command)
            with (folder/'player.log').open('wb') as log:result=subprocess.run(command,stdout=log,stderr=log,timeout=150,cwd=folder)
            assert result.returncode==0,(name,result.returncode)
            log=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I),name
            diagnostics=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()]
            assert not any(row['level']=='error' for row in diagnostics),name
            assert any(row['message']=='Allocator after unload: buffers=0 images=0' for row in diagnostics),name
            if expected_validation!='any':assert ('Validation layer: enabled' in log)==(expected_validation=='enabled'),name
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text())
            assert manifest['materialSurfaceProfileLayoutVersion']==21 and manifest['qaStateFingerprintVersion']==4,name
            assert manifest['sceneDisplayMaskFormat']=='R8_UNORM' and manifest['sceneDisplayMaskBytes']>0,name
            overrides=manifest['materialDisplayTransforms']
            assert bool(overrides)==(mode=='linear'),(name,overrides)
            assert all(row['displayTransform']=='linear' for row in overrides),name
            assert manifest['capturedFrames']==definition['frames'],name
            if family=='multisample':assert manifest['snapshotRasterizationSamples']==4 and manifest['snapshotMultisampleBytes']>0,name
            if family=='temporal':
                assert manifest['temporalDisplayMaskFormat']=='R16G16_SFLOAT' and manifest['temporalDisplayMaskBytes']==128*128*8,name
                assert manifest['temporalBytes']==128*128*60 and not manifest['temporalFallback'],name
            runtime=json.loads((folder/'runtime.json').read_text());assert runtime['scriptErrors']==[] and runtime['presentationErrors']==[],name
            path=folder/'frames'/f"frame_{definition['frames']-1:06}.png";images[name]=np.array(Image.open(path).convert('RGB'))
            row=dict(name=name,command=command,imageSha256=digest(path),frames=definition['frames'],allocatorUnloadVerified=True)
            report['captures'].append(row);assert digest(player)==program
            print('CAPTURED',name,flush=True);save(output/'summary.json',report)
        for family in FAMILIES:
            ref,target,scene=[family+'-'+mode for mode in ('reference','linear','scene')]
            if ref not in images or target not in images:continue
            a,b=images[ref],images[target]
            if family in ('cutout','opacity'):
                foreground=np.any(a!=a[0,0],axis=2);assert foreground.sum()>128 and (~foreground).sum()>128,family
                error=float(np.abs(linear(a[foreground])-linear(b[foreground])).max())
                clear_error=float(np.abs(linear(b[~foreground])-CONSTANT).max());assert clear_error<=BUDGET,(family,'Clear region policy',clear_error)
            elif family=='outline':
                foreground=np.any(a!=a[0,0],axis=2);assert foreground.sum()>128
                error=float(np.abs(linear(a[foreground])-linear(b[foreground])).max())
            elif family=='mixed':
                error=float(np.abs(linear(a[32:96,20:44])-linear(b[32:96,20:44])).max())
                assert scene in images and np.array_equal(b[16:112,80:112],images[scene][16:112,80:112]),'Scene surface guard changed'
            elif family in ('multisample','temporal'):
                aa=linear(a);bg=aa[0,0];center=aa[64,64];delta=center-bg;assert np.dot(delta,delta)>.001
                coverage=np.clip(np.sum((aa-bg)*delta,axis=2)/np.dot(delta,delta),0,1)
                if family=='multisample':coverage=np.round(coverage*4)/4
                assert ((coverage>.01)&(coverage<.99)).sum()>20
                expected=aa*coverage[...,None]+CONSTANT*(1-coverage[...,None])
                error=float(np.abs(expected-linear(b)).max())
            else:
                pixels=definitions[target]['pixels']
                if family=='temporal':pixels=[[48,64],[64,64],[80,64]]
                expected=np.array([linear(a[y,x]) for x,y in pixels])
                if family=='transparent':expected=expected*.5+CONSTANT*.5
                actual=np.array([linear(b[y,x]) for x,y in pixels]);error=float(np.abs(expected-actual).max())
            report['comparisons'].append(dict(family=family,maximumLinearError=error));assert error<=BUDGET,(family,error)
            if scene in images and family not in ('cutout','opacity','mixed','outline','multisample','temporal'):
                points=definitions[scene]['pixels'];actual=np.array([linear(images[scene][y,x]) for x,y in points])
                assert np.abs(actual-CONSTANT).max()<=BUDGET,(family,'Scene LUT control is not active')
        for family in ('npr','npr-document','eye','hair'):
            a,b=family+'-linear',family+'-linear-fixed'
            if a in images and b in images:assert np.array_equal(images[a],images[b]),(family,'Fixed descriptor path differs')
        if 'unlit-explicit-scene' in images and 'unlit-scene' in images:
            assert np.array_equal(images['unlit-explicit-scene'],images['unlit-scene']),'Explicit default changes pixels'
        if 'eye-linear' in images:
            definition=definitions['eye-linear'];expected=np.array([row['linearRgb'] for row in definition['oracle']])*.25
            actual=np.array([linear(images['eye-linear'][y,x]) for x,y in definition['pixels']])
            report['independentEyeOracleError']=float(np.abs(expected-actual).max());assert report['independentEyeOracleError']<=BUDGET
        assert shaders=={p.name:digest(p) for p in sorted((player.parent/'shaders').glob('*.spv'))}
        report['status']='passed';print(json.dumps({'status':'passed','captures':len(images),'comparisons':len(report['comparisons'])}))
    except Exception as error:report.update(status='failed',failure=str(error));raise
    finally:save(output/'summary.json',report)

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--case',action='append',default=[]);parser.add_argument('--expected-validation',choices=('any','enabled','disabled'),default='any');args=parser.parse_args()
    owned=None
    if args.output is None:
        owned=Path(subprocess.check_output([sys.executable,str(ROOT/'tools/artifact_cleanup.py'),'--new-run','material-display-gpu'],text=True,cwd=ROOT).strip());args.output=owned/'captures'
    try:run(args.executable.resolve(),args.output.resolve(),args.case,args.expected_validation)
    finally:
        if owned is not None:subprocess.run([sys.executable,str(ROOT/'tools/artifact_cleanup.py'),'--finish',str(owned)],check=True,cwd=ROOT)
