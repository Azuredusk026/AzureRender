"""Exercise bounded object shadows through public scene and material contracts."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import uuid
import numpy as np
from PIL import Image
from native_render.light_lab import generate
from convert_material_semantics import complete_material_document


def component(kind,data):
    return dict(type=kind,version=1,data=data)


def hair_receiver(receiver,model,folder,source,lit=False):
    images=[('shadow.png',(255,255,255,255)),('packed.png',(128,128,128,128)),
            ('parameters.png',(128,255 if lit else 0,255,255 if lit else 0)),
            ('hair-ramp.png',(255,255,255,255))]
    model['images']=[];model['textures']=[]
    for name,rgba in images:
        Image.new('RGBA',(2,2) if name=='hair-ramp.png' else (2,1),rgba).save(folder/name)
        model['images'].append(dict(uri=name));model['textures'].append(dict(source=len(model['images'])-1))
    lighting=dict(schemaVersion=1,shadowBrightness=.55,shadowSaturation=1.1,shadowScale=.55,
                  ambientColor=[0,0,0],mainColor=[1,1,1])
    if source is not None:lighting['characterVisibility']=source
    projected=dict(schemaVersion=1,primaryShift=0,secondaryShift=0,intensity=1,secondaryRange=1,
                   edgeFade=1,specularNormalScale=1,surfaceNormalScale=1,directionX=0,
                   parameterTexture=2,rampTexture=3,secondaryColor=[.25,.4,.7],
                   objectBasis=[1,0,0,0,1,0,0,0,1],surfaceResponse='strand-diffuse',diffuseScale=.96,
                   lighting=lighting)
    receiver['extras']['azureRenderMaterial']=dict(schemaVersion=2,**{'class':'hair'},
        features=['hair-anisotropy','hair-projected-ramp','diffuse-ramp'],
        diffuseRamp=dict(schemaVersion=1,texture=0),
        hair=dict(power=200,strength=1,rampRow=3,strandShift=0,upperLimit=4,
                  dataTexture=1,dataNormalY='positive',projectedRamp=projected))


def run(player,output,selected=None,expected_validation=None):
    output.mkdir(parents=True,exist_ok=False)
    names=['clear','shadow','shadow-fixed','shadow-document','disabled','mirrored-clear','mirrored','two-objects','ordinary','ordinary-disabled']
    names+=['visibility','visibility-lit','visibility-disabled']
    names+=['receiver-zero','receiver-offset','receiver-normal','receiver-combined','receiver-fixed','receiver-document']
    names+=['one','hair-legacy','hair-one','hair-object','hair-object-fixed','hair-object-document',
            'hair-object-disabled','hair-clear','hair-one-lit','hair-object-lit','hair-object-ambient','hair-object-ambient-ms']
    if selected:
        assert set(selected)<=set(names);names=[name for name in names if name in selected]
    digest=hashlib.sha256(player.read_bytes()).hexdigest();frames={}
    report=dict(schemaVersion=1,status='running',playerSha256=digest,captures=[])
    try:
        for name in names:
            print(name,flush=True);folder=output/name;generate(folder,'directional-near')
            model=json.loads((folder/'plane.gltf').read_text(encoding='utf-8'))
            receiver=model['materials'][0];receiver['doubleSided']=True
            receiver.setdefault('extras',{})['azureSlotId']='surface'
            receiver['pbrMetallicRoughness']['baseColorFactor']=[.4,.3,.2,1]
            if not name.startswith('ordinary'):
                Image.new('RGBA',(16,4),(40,30,20,255)).save(folder/'shadow.png')
                model['images']=[dict(uri='shadow.png')];model['textures']=[dict(source=0)]
                receiver['extras']['azureRenderMaterial']=dict(schemaVersion=2,**{'class':'skin'},features=[],
                    shadowColorLut=dict(schemaVersion=1,texture=0,flipV=False),
                    nprDiffuse=dict(schemaVersion=1,shadowScale=.55,lightingInputs='host-scene-radiance',
                        shadowMask='one',baseAo='one',characterVisibility='object-shadow'))
            clear_model=copy.deepcopy(model)
            if name=='one':receiver['extras']['azureRenderMaterial']['nprDiffuse']['characterVisibility']='one'
            if name.startswith('hair-'):
                source=None if name=='hair-legacy' else 'object-shadow' if name.startswith('hair-object') else 'one'
                hair_receiver(receiver,model,folder,source,name.endswith('-lit'))
                if name.startswith('hair-object-ambient'):
                    lighting=receiver['extras']['azureRenderMaterial']['hair']['projectedRamp']['lighting']
                    lighting.update(ambientColor=[1,1,1],mainColor=[0,0,0])
                if name.endswith('-ms'):receiver['extras']['azureRenderMaterial']['features'].append('hair-pixel-integral')
            if name.startswith('receiver-'):
                Image.new('RGBA',(4,4),(230,128,205,255)).save(folder/'normal.png')
                model['images'].append(dict(uri='normal.png'));model['textures'].append(dict(source=1))
                receiver['normalTexture']=dict(index=1)
            if name not in ['clear','mirrored-clear','visibility-lit','hair-clear','hair-one-lit','hair-object-lit']:
                model['nodes'].append(dict(mesh=0,translation=[0,0,1],scale=[.125,.125,1]))
                model['scenes'][0]['nodes'].append(1)
            (folder/'plane.gltf').write_text(json.dumps(model),encoding='utf-8')
            level_path=folder/'level.azurelevel';level=json.loads(level_path.read_text(encoding='utf-8'))
            level['lights'][0]['shape']['direction']=[.6,0,-.8]
            level['renderSettings'].update(sceneLighting=True,skyIntensity=0,shadowDistance=20,
                nprLighting=dict(schemaVersion=1,enabled=True,ambientDirection=[0,0,1],directionBias=0,
                    directionGain=1,directionFloor=0,litAmbientMix=0,ambientColor=[0,0,0],
                    shadowOverride=1,indoorMix=0,useSceneIrradiance=False))
            if name.startswith('hair-object-ambient'):
                level['renderSettings']['nprLighting'].update(ambientWeight=1,outdoorGain=1)
            receiver_node=next(node for node in level['nodes'] if node.get('resourceId')=='plane')
            settings=dict(enabled=not name.endswith('disabled'),center=[0,0,.5],halfExtents=[6,6,3],
                normalBias=.1,lightBias=.05,filterRadiusX=4,filterRadiusY=2,atlasPage=6)
            receiver_node['components']={'azure.object-shadow':component('azure.object-shadow',settings)}
            if name.startswith('receiver-'):
                settings.update(receiverPixelOffsetX=0 if name in ['receiver-zero','receiver-normal'] else -2,
                    receiverPixelOffsetY=0 if name in ['receiver-zero','receiver-normal'] else 2,
                    surfaceNormalBias=name not in ['receiver-zero','receiver-offset'])
            if name in ['shadow-document','receiver-document','hair-object-document']:
                material_id=str(uuid.uuid4());texture_ids={}
                for index,image in enumerate(model['images']):
                    identity=str(uuid.uuid4());texture_ids[str(index)]=identity
                    (folder/(image['uri']+'.azmeta')).write_text(json.dumps(dict(schemaVersion=1,id=identity,settings={},dependencies=[])))
                (folder/'surface.azurematerial').write_text(json.dumps(complete_material_document(receiver,model,texture_ids)))
                (folder/'surface.azurematerial.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,settings={},dependencies=list(texture_ids.values()))))
                receiver_node['materialSlots']={'surface':material_id}
            if name.startswith('mirrored'):receiver_node['components']['azure.transform']=component('azure.transform',dict(scale=[-1,1,1]))
            if name=='two-objects':
                receiver_node['components']['azure.transform']=component('azure.transform',dict(translation=[2,0,0],scale=[.4,.4,.4]))
                other=copy.deepcopy(receiver_node);other.update(id='second',resourceId='second')
                other['components']['azure.transform']['data']['translation']=[-2,0,0]
                other['components']['azure.object-shadow']['data']['atlasPage']=7
                level['nodes'].append(other);level['resources'].append(dict(id='second',asset='assets:/second.gltf'))
                (folder/'second.gltf').write_text(json.dumps(clear_model),encoding='utf-8')
                (folder/'second.gltf.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=str(uuid.uuid4()),settings={},dependencies=[])))
            level_path.write_text(json.dumps(level,indent=2),encoding='utf-8')
            command=[str(player),'--project',str(folder/'lab.azureproject'),'--width','256','--height','256',
                '--capture-dir',str(folder/'frames'),'--capture-frames','2','--fixed-frame-step','--qa-camera','scene',
                '--qa-light','scene','--runtime-report',str(folder/'runtime.json')]
            if name.endswith('-lit'):command+=['--qa-isolation','hair-kk']
            else:command+=['--qa-effect','specular','--qa-effect-state','disabled']
            if name.startswith('hair-object-ambient'):command+=['--qa-isolation','ambient','--qa-exposure','0']
            if name.endswith('-ms'):command+=['--anti-aliasing','multisample']
            if name.endswith('fixed'):command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            if name.startswith(('visibility','receiver-')):command+=['--qa-isolation','object-shadow-visibility']
            (folder/'command.json').write_text(json.dumps(command,indent=2),encoding='utf-8')
            with (folder/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log,name
            validation='Validation layer: enabled' in log
            if expected_validation:assert validation==(expected_validation=='enabled'),name
            runtime=json.loads((folder/'runtime.json').read_text(encoding='utf-8'));assert not runtime['scriptErrors'] and not runtime['presentationErrors']
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text(encoding='utf-8'))
            pages=0 if name.endswith('disabled') else 2 if name=='two-objects' else 1
            assert manifest['objectShadowLayoutVersion']==2 and manifest['objectShadowPages']==pages
            assert manifest['objectShadowDepthBytes']==(16*1024*1024 if pages else 0)
            assert manifest['instanceDataLayoutVersion']==2 and manifest['materialSurfaceProfileLayoutVersion']==12
            if name.endswith('-ms'):assert manifest['rasterizationSamples']==4
            frame=folder/'frames/frame_000001.png';pixels=np.asarray(Image.open(frame).convert('RGB'));frames[name]=pixels
            assert np.array_equal(pixels,np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))),name
            target=pixels[126:130,200:204] if name=='two-objects' else pixels[126:130,150:154]
            if name.startswith('visibility'):
                assert manifest['qaIsolation']=='object-shadow-visibility'
                assert pixels[124:132,84:92].min()==255,name
                if name=='visibility':assert target.max()==0,name
                else:assert target.min()==255,name
            if name.startswith('hair-object-ambient'):
                assert target.min()>16,'Character occlusion must preserve the independent ambient incident weight'
            report['captures'].append(dict(name=name,validationEnabled=validation,objectPages=pages,
                center=pixels[128,152].tolist(),shadowMean=float(target.mean()),
                control=pixels[128,88].tolist(),imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest()))
        for name in ['shadow','shadow-fixed','mirrored']:
            reference='mirrored-clear' if name=='mirrored' else 'clear'
            if name in frames and reference in frames:
                assert frames[name][126:130,150:154].mean()<frames[reference][126:130,150:154].mean()*.85,name
                assert np.array_equal(frames[name][124:132,84:92],frames[reference][124:132,84:92]),name
        for a,b in [('shadow','shadow-fixed'),('shadow','shadow-document'),('ordinary','ordinary-disabled')]:
            if a in frames and b in frames:assert np.array_equal(frames[a],frames[b]),(a,b)
        for name in ['hair-object','hair-object-fixed','hair-object-document']:
            if name in frames and 'hair-one' in frames:
                assert frames[name][126:130,150:154].mean()<frames['hair-one'][126:130,150:154].mean()*.85,name
                assert np.array_equal(frames[name][124:132,84:92],frames['hair-one'][124:132,84:92]),name
        for a,b in [('one','disabled'),('hair-legacy','hair-one'),('hair-object','hair-object-fixed'),
                    ('hair-object','hair-object-document'),('hair-object-disabled','hair-one'),
                    ('hair-one-lit','hair-object-lit')]:
            if a in frames and b in frames:assert np.array_equal(frames[a],frames[b]),(a,b)
        if 'hair-one-lit' in frames:
            assert frames['hair-one-lit'][124:132,84:92].max()>4,'Virtual-light-disabled control must retain positive KK energy'
        for name in ['receiver-offset','receiver-normal','receiver-combined']:
            if name in frames and 'receiver-zero' in frames:
                assert np.any(frames[name]!=frames['receiver-zero']),name
        for name in ['receiver-fixed','receiver-document']:
            if name in frames and 'receiver-combined' in frames:
                assert np.array_equal(frames[name],frames['receiver-combined']),name
        if 'disabled' in frames and 'clear' in frames:
            assert np.array_equal(frames['disabled'][124:132,148:156],frames['clear'][124:132,148:156])
        if 'two-objects' in frames:
            image=frames['two-objects'];assert image[126:130,200:204].mean()<image[126:130,72:76].mean()*.85
        assert hashlib.sha256(player.read_bytes()).hexdigest()==digest
        report['status']='passed'
    finally:(output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--case',action='append');parser.add_argument('--expected-validation',choices=['enabled','disabled'])
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.case,args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='azure_object_shadow_') as directory:run(args.executable.resolve(),Path(directory)/'captures',args.case,args.expected_validation)
