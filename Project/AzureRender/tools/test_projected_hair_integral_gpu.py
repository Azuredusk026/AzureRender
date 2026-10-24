"""Compare projected-hair pixel integration with independent 4x sampling."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image
import test_projected_hair_gpu as hair


def fixture(folder,coupled,integrated,line_diffuse=False,incident=False,composed=False):
    case=hair.cases()[0];case['name']='pixel-integral';project,_=hair.prepare(case,folder)
    path=folder/'assets/panels.gltf';asset=json.loads(path.read_text())
    blob=bytearray(base64.b64decode(asset['buffers'][0]['uri'].split(',',1)[1]))
    y,x=np.mgrid[:512,:512]
    parameters=np.zeros((512,512,4),dtype=np.uint8)
    parameters[:,:,0]=128 if not coupled else np.clip(128+24*np.sin(x*.12+y*.07),0,255)
    parameters[:,:,1]=np.where(((x//3+y//5)%2)==0,48,240)
    parameters[:,:,3]=np.where((x//7+y//4)%2,96,192) if coupled else 0
    normals=np.full((512,512,4),128,dtype=np.uint8)
    if coupled:
        normals[:,:,2]=128+np.rint(24*np.sin(x*.09+y*.04)).astype(np.int16)
        normals[:,:,3]=128+np.rint(20*np.cos(y*.1)).astype(np.int16)
    if incident or composed:
        normals[:,:,0]=128+np.rint(76*np.sin(x*.09+y*.03)).astype(np.int16)
        normals[:,:,1]=128+np.rint(72*np.cos(y*.1-x*.05)).astype(np.int16)
    if composed:parameters[:,:,2]=np.where(parameters[:,:,1]>128,240,64)
    def texture(pixels):
        image=len(asset['images']);asset['images'].append({'uri':hair.image_uri(Image.fromarray(pixels,'RGBA'))})
        index=len(asset['textures']);asset['textures'].append({'source':image});return index
    parameter=texture(parameters);normal=texture(normals)
    stripe=np.zeros((512,512,4),dtype=np.uint8);stripe[:,:,:3]=np.where((x%11)<5,32,224)[:,:,None];stripe[:,:,3]=255
    line=texture(stripe)
    diffuse=texture(np.full((1,2,4),255,dtype=np.uint8))
    for primitive in asset['meshes'][0]['primitives']:
        accessor=asset['accessors'][primitive['attributes']['TEXCOORD_0']];view=asset['bufferViews'][accessor['bufferView']]
        offset=view.get('byteOffset',0)+accessor.get('byteOffset',0)
        struct.pack_into('<8f',blob,offset,0,0,1,0,1,1,0,1)
    asset['buffers'][0]['uri']='data:application/octet-stream;base64,'+base64.b64encode(blob).decode()
    for material in asset['materials']:
        profile=material['extras']['azureRenderMaterial'];profile['hair']['dataTexture']=normal
        p=profile['hair']['projectedRamp'];p['parameterTexture']=parameter
        p['specularNormalScale']=1 if coupled else 0
        p['secondaryRange']=.5;p['secondaryColor']=[.6,.2,.3]
        if integrated:profile['features'].append('hair-pixel-integral')
        if coupled:p['line']={'schemaVersion':1,'texture':line,'frequency':80,'strandShift':.4,'range':.95,
            'depth':.22,'alternation':1,'saturation':1.5,'uvTransform':[1,1,0,0]}
        if line_diffuse or incident or composed:
            p['surfaceResponse']='strand-diffuse'
            profile['features'].append('diffuse-ramp');profile['diffuseRamp']={'schemaVersion':1,'texture':diffuse}
            p['lighting']={'schemaVersion':1,'shadowBrightness':1,'shadowSaturation':1,'shadowScale':1,
                'ambientColor':[0,0,0] if incident else [.02,.02,.02],'mainColor':[0,0,0] if incident else [1,1,1]}
        if line_diffuse:
            p['line']['diffuseTarget']='main-and-ambient';p['line']['range']=1;p['line']['depth']=.8
        if incident or composed:p['surfaceNormalScale']=1
    path.write_text(json.dumps(asset))
    if line_diffuse or incident or composed:
        level=folder/'assets/scene.azurelevel';document=json.loads(level.read_text())
        document['renderSettings']['nprLighting']={'schemaVersion':1,'enabled':True}
        if incident:
            document['nodes'].append({'id':'local','components':{'azure.transform':{'type':'azure.transform','version':1,'data':{'translation':[0,.16,4]}}}})
            document['lights'].append({'id':'local','nodeId':'local','color':[.4,.7,1],'intensity':16,'radius':10,'enabled':True,
                'shape':{'type':'point','falloff':'smooth-inverse-square','characterResponse':{'schemaVersion':1,'diffuseFloor':.25,'compression':1}}})
        level.write_text(json.dumps(document))
    return project


def linear(pixels):
    a=pixels.astype(float)/255
    return np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)


def run(player,output,validation,selected=None):
    output.mkdir(parents=True,exist_ok=False);digest=hashlib.sha256(player.read_bytes()).hexdigest()
    report=dict(schemaVersion=1,status='running',playerSha256=digest,captures=[],quality=[])
    try:
        for name,coupled,line_diffuse,incident,composed in (('primary',False,False,False,False),('coupled',True,False,False,False),('line',True,True,False,False),('incident',True,False,True,False),('composed',True,False,False,True)):
            if selected and name not in selected:continue
            images={}
            for kind,scale,integrated in (('source',1,False),('integral',1,True),('reference',4,False)):
                folder=output/f'{name}-{kind}';project=fixture(folder,coupled,integrated,line_diffuse,incident,composed)
                command=[str(player),'--project',str(project),'--width',str(128*scale),'--height',str(128*scale),
                    '--capture-dir',str(folder/'frames'),'--capture-frames','1','--fixed-frame-step',
                    '--qa-camera','scene','--qa-light','scene','--qa-isolation','direct-diffuse' if line_diffuse else 'hair-kk','--qa-exposure','-4']
                if coupled:command+=['--disable-bindless','--disable-compute-skinning']
                (folder/'command.json').write_text(json.dumps(command,indent=2))
                with (folder/'player.log').open('w',encoding='utf-8') as log:
                    subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
                manifest=json.loads((folder/'frames/capture_manifest.json').read_text(encoding='utf-8-sig'))
                assert manifest['pixelIntegralPipelineCount']==(6 if integrated else 0),(kind,manifest['pixelIntegralPipelineCount'])
                log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
                assert 'Allocator after unload: buffers=0 images=0' in log,name
                assert ('Validation layer: enabled' in log)==(validation=='enabled'),name
                path=folder/'frames/frame_000000.png';pixels=np.asarray(Image.open(path).convert('RGB'))
                assert pixels.max()<255,('Radiometric reference must remain unsaturated',name,kind)
                images[kind]=linear(pixels)
                report['captures'].append(dict(name=name,kind=kind,imageSha256=hashlib.sha256(path.read_bytes()).hexdigest()))
            ref=images['reference'].reshape(128,4,128,4,3).mean((1,3))
            for side,box in [('left',(32,32,59,96)),('right',(69,32,96,96))]:
                x0,y0,x1,y1=box;reference=ref[y0:y1,x0:x1];source=images['source'][y0:y1,x0:x1];candidate=images['integral'][y0:y1,x0:x1]
                energy=float(reference.sum());assert energy>1
                ratio=float(candidate.sum()/energy);source_ratio=float(source.sum()/energy)
                source_error=float(np.abs(source-reference).mean());error=float(np.abs(candidate-reference).mean())
                assert source_error>1e-6,('Source control must exhibit a measurable footprint error',name,side,source_error)
                report['quality'].append(dict(name=name,side=side,energyRatio=ratio,sourceEnergyRatio=source_ratio,
                    meanAbsoluteError=error,sourceMeanAbsoluteError=source_error,errorRatio=error/source_error))
                if not coupled:assert abs(source_ratio-1)>.02,('Source control must expose nonlinear footprint aliasing',side,source_ratio)
                assert abs(ratio-1)<=.02,('Integrated response must retain reference energy within 2 percent',name,side,ratio)
                assert error/source_error<=.8,('Integration must reduce reference error to at most 80 percent',name,side,error/source_error)
        assert hashlib.sha256(player.read_bytes()).hexdigest()==digest;report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(output/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['quality']))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--validation',choices=('enabled','disabled'),required=True)
    parser.add_argument('--case',choices=('primary','coupled','line','incident','composed'),action='append');args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.validation,args.case)
    else:
        with tempfile.TemporaryDirectory(prefix='azure-hair-integral-') as temporary:
            run(args.executable.resolve(),Path(temporary)/'captures',args.validation,args.case)
