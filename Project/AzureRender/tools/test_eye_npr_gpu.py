"""Verify the public NPR eye adapter with constant-color oracles and mip controls."""
import argparse
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
from create_visibility_fixture import pad_material_profile_index


def linear(rgb):
    x=np.asarray(rgb,dtype=float)/255
    return np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)


def run(player,output,selected=None,validation=None):
    names=['ambient','main','combined','dark','shadow-zero','shadow-one','matcap-rgba','matcap-rgb',
           'matcap-alpha','matcap-dark','fixed','document','mip-low','mip-high','high-chroma']
    if selected:
        assert set(selected)<=set(names);names=[x for x in names if x in selected]
    output.mkdir(parents=True,exist_ok=False);digest=hashlib.sha256(player.read_bytes()).hexdigest()
    report=dict(schemaVersion=1,status='running',playerSha256=digest,linearErrorBudget=.008,captures=[])
    frames={};maximum=0
    try:
        for name in names:
            print(name,flush=True);folder=output/name;generate(folder,'directional-near')
            model=json.loads((folder/'plane.gltf').read_text(encoding='utf-8'));material=model['materials'][0]
            ambient=np.array([.25,.5,.75]) if name not in ['main','dark','matcap-dark'] else np.zeros(3)
            main=np.full(3,.4) if name in ['main','combined','fixed','document'] else np.zeros(3)
            matcap=name.startswith(('matcap-','mip-'));base=[0,0,0,255] if matcap else [128,64,224,255]
            if name=='high-chroma':ambient=np.full(3,16.);base=[32,64,224,255]
            mc=[64,128,192,128] if matcap else [0,0,0,0]
            if name=='matcap-rgb':mc[3]=0
            if name=='matcap-alpha':mc[:3]=[0,0,0]
            for filename,value in [('base.png',base),('ramp.png',[255,255,255,128]),('matcap.png',mc)]:
                Image.new('RGBA',(4,1),tuple(value)).save(folder/filename)
            source_images=[dict(uri=x) for x in ['base.png','ramp.png','matcap.png']]
            model['images']=source_images;model['textures']=[dict(source=i,sampler=0) for i in range(3)]
            model['samplers']=[dict(wrapS=33071,wrapT=33071,magFilter=9729,minFilter=9729)]
            color=[.2,.3,.4,.5];shadow=0 if name=='shadow-zero' else 1 if name=='shadow-one' else .55
            eye=dict(schemaVersion=1,diffuseRampTexture=1,matcapTexture=2,parallaxScale=0,
                matcapNormalScale=0,alphaPremultiply=0,highlightColor=[1,1,1],scatteringColor=[1,1,1],
                matcapColor=color,uvScaleOffset=[1,1,0,0],lightingInputs=dict(source='host-authored',
                    illuminationModel='npr-main-ambient',matcapShadowFloor=.55,directWeights=[0,0,0],
                    ambientColor=ambient.tolist(),mainColor=main.tolist(),shadowScale=shadow))
            material.clear();material.update(name='public-eye',doubleSided=True,alphaMode='OPAQUE',
                pbrMetallicRoughness=dict(baseColorTexture=dict(index=0),metallicFactor=0,roughnessFactor=1),
                extras=dict(azureSlotId='eye',azureRenderMaterial={'schemaVersion':2,'class':'eye',
                    'features':['eye-radial-surface'],'eyeSurface':eye}))
            if name.startswith('mip-'):
                y,x=np.indices((64,64));checker=((x+y)%2*255).astype(np.uint8)
                Image.fromarray(np.stack([checker,255-checker,checker,np.full_like(checker,255)],axis=2)).save(folder/'matcap.png')
                eye['matcapNormalScale']=1;eye['matcapColor']=[0,0,0,1]
                model['samplers'].append(dict(wrapS=33071,wrapT=33071,magFilter=9729,minFilter=9987))
                model['textures'][2]['sampler']=1
                (folder/'matcap.png.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=str(uuid.uuid4()),dependencies=[],
                    settings=dict(texture=dict(schemaVersion=1,usage='albedo',colorSpace='srgb',filter='linear',
                        wrapU='clamp',wrapV='clamp',mipPolicy='generate',anisotropy=1,mipBias=-1 if name=='mip-low' else 2)))),encoding='utf-8')
            (folder/'plane.gltf').write_text(json.dumps(model),encoding='utf-8')
            level_path=folder/'level.azurelevel';level=json.loads(level_path.read_text(encoding='utf-8'))
            level['lights'][0]['intensity']=1
            level['renderSettings'].update(sceneLighting=True,skyIntensity=0,nprLighting=dict(schemaVersion=1,enabled=True,
                ambientDirection=[0,0,1],directionBias=0,directionGain=0,directionFloor=1,litAmbientMix=0,
                ambientColor=[1,1,1],shadowOverride=1,indoorMix=0,useSceneIrradiance=False,
                outdoorGain=1,shadowAmbientGain=1,mainColorMix=0,ambientWeight=1))
            if name=='document':
                ids={str(i):str(uuid.uuid4()) for i in range(3)};material_id=str(uuid.uuid4())
                for i,image in enumerate(source_images):
                    (folder/(image['uri']+'.azmeta')).write_text(json.dumps(dict(schemaVersion=1,id=ids[str(i)],settings={},dependencies=[])),encoding='utf-8')
                (folder/'eye.azurematerial').write_text(json.dumps(complete_material_document(material,model,ids)),encoding='utf-8')
                (folder/'eye.azurematerial.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,settings={},dependencies=list(ids.values()))),encoding='utf-8')
                next(x for x in level['nodes'] if x.get('resourceId')=='plane')['materialSlots']={'eye':material_id}
            level_path.write_text(json.dumps(level),encoding='utf-8')
            pad_material_profile_index(model);(folder/'plane.gltf').write_text(json.dumps(model),encoding='utf-8')
            command=[str(player),'--project',str(folder/'lab.azureproject'),'--width','128','--height','128',
                '--capture-dir',str(folder/'frames'),'--capture-frames','2','--fixed-frame-step','--qa-camera','scene',
                '--qa-light','scene','--runtime-report',str(folder/'runtime.json')]
            if name=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            with (folder/'player.log').open('w',encoding='utf-8') as log:
                subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log,name
            if validation:assert ('Validation layer: enabled' in log)==(validation=='enabled'),name
            runtime=json.loads((folder/'runtime.json').read_text(encoding='utf-8'));assert not runtime['scriptErrors'] and not runtime['presentationErrors']
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text(encoding='utf-8'))
            assert manifest['materialSurfaceProfileLayoutVersion']==16
            frame=folder/'frames/frame_000001.png';pixels=np.asarray(Image.open(frame).convert('RGB'));frames[name]=pixels
            assert np.array_equal(pixels,np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))),name
            error=None;expected=None
            if not name.startswith('mip-'):
                alpha=128/255;illum=ambient+main
                diffuse=linear(base[:3])*.96*(shadow+(1-shadow)*alpha)*illum
                cap=(linear(mc[:3])*color[3]+mc[3]/255*np.asarray(color[:3]))*(.5*alpha+.5)*(alpha*.45+.55)*illum
                hdr=diffuse+cap;luma=float(hdr@np.array([.21267290413379669,.7151522040367126,.07217500358819962]))
                saturation=1+min(max(luma-.5,0),.5)**2
                expected=np.clip(luma+(hdr-luma)*saturation,0,1)
                actual=linear(pixels[60:69,60:69]);error=float(np.abs(actual-expected).max());maximum=max(maximum,error)
                assert error<=.008,(name,error,actual[4,4],expected)
            report['captures'].append(dict(name=name,maximumLinearError=error,expected=None if expected is None else expected.tolist(),
                imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest()))
        for name in ['fixed','document']:
            if name in frames and 'combined' in frames:assert np.array_equal(frames[name],frames['combined']),name
        if 'mip-low' in frames and 'mip-high' in frames:
            changed=int(np.any(frames['mip-low']!=frames['mip-high'],axis=2).sum());assert changed>32,changed
            report['mipChangedPixels']=changed
        assert hashlib.sha256(player.read_bytes()).hexdigest()==digest
        report.update(status='passed',maximumLinearError=maximum)
    finally:(output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--case',action='append')
    parser.add_argument('--expected-validation',choices=['enabled','disabled']);args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.case,args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='azure_eye_npr_') as directory:
            run(args.executable.resolve(),Path(directory)/'captures',args.case,args.expected_validation)
