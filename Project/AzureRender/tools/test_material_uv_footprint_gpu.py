"""Expose the reference primary-UV footprint with authored mip colors."""
import argparse,base64,hashlib,json,os,re,struct,subprocess,uuid
from pathlib import Path
import numpy as np
from PIL import Image
from test_material_coverage_gpu import prepare as coverage_fixture

def save(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def prepare(folder,enabled):
    project=coverage_fixture(folder,'generic','reference','red',0)
    path=folder/'surface.gltf';model=json.loads(path.read_text());materialIndex=model['meshes'][0]['primitives'][0]['material'];material=model['materials'][materialIndex]
    material['alphaMode']='OPAQUE';material.setdefault('extras',{})['azureRenderMaterial']={'schemaVersion':2,'class':'generic','features':['mirrored-coarse-uv'] if enabled else []}
    save(folder/'fixture.json',{'schemaVersion':2,'materialIndex':materialIndex,'enabled':enabled,'planeTopY':4,'profile':material['extras']['azureRenderMaterial']})
    raw=bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',')[1]))
    positions=[[-2,-1,2],[2,-1,-4],[2,4,-4],[-2,4,2]]
    raw[:48]=struct.pack('<12f',*(v for point in positions for v in point))
    model['accessors'][0].update(min=[-2,-1,-4],max=[2,4,2])
    uv_accessor=model['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0'];view=model['bufferViews'][model['accessors'][uv_accessor]['bufferView']];offset=view.get('byteOffset',0)
    raw[offset:offset+32]=struct.pack('<8f',0,0,0,0,0,16,0,16)
    model['buffers'][0]['uri']='data:application/octet-stream;base64,'+base64.b64encode(raw).decode();save(path,model)
    # Each authored level has an independent color; the trilinear result exposes
    # changes in the footprint without changing texture contents or geometry.
    dds=folder/'coverage.dds';header=dds.read_bytes()[:148];palette=[(0,0,255,255),(255,0,0,255),(0,255,0,255),(255,0,255,255),(0,0,0,255)]
    dds.write_bytes(header+b''.join(bytes(color)*(max(16>>level,1)**2) for level,color in enumerate(palette)))
    save(folder/'coverage.dds.azmeta',{'schemaVersion':1,'id':str(uuid.uuid4()),'dependencies':[],'settings':{'texture':{'usage':'albedo','colorSpace':'linear','filter':'linear','mipFilter':'linear','anisotropy':1}}})
    level_path=folder/'scene.azurelevel';level=json.loads(level_path.read_text())
    camera=next(n for n in level['nodes'] if 'azure.camera' in n.get('components',{}));camera['components']['azure.camera']['data'].update(orthographic=False,fieldOfView=55)
    save(level_path,level);return project

def run(player,output,validation):
    output.mkdir(parents=True,exist_ok=False);rows=[];images={};digest=hashlib.sha256(player.read_bytes()).hexdigest();result={'status':'running','programSha256':digest,'cases':rows}
    try:
        for backend in ['indexed','fixed']:
            for enabled in [False,True]:
                label=backend+('-reference' if enabled else '-device');folder=output/label;project=prepare(folder,enabled)
                command=[str(player),'--project',str(project),'--width','128','--height','128','--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','albedo','--capture-dir',str(folder/'frames'),'--runtime-report',str(folder/'runtime.json')]
                if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                save(folder/'command.json',command);print('START mip footprint',label,flush=True)
                with (folder/'player.log').open('wb') as log:r=subprocess.run(command,cwd=folder,stdout=log,stderr=log,timeout=180)
                text=(folder/'player.log').read_text(errors='replace');assert r.returncode==0,(label,text[-2000:]);assert not re.search('VUID-|SYNC-HAZARD|Validation (?:Error|Warning)|\\[error\\]',text),label
                assert 'Allocator after unload: buffers=0 images=0' in text,label
                if validation!='any':assert ('Validation layer: enabled' in text)==(validation=='enabled'),label
                image_path=folder/'frames/frame_000000.png';images[label]=np.asarray(Image.open(image_path).convert('RGB'))
                capability=json.loads((folder/'frames/gpu_capabilities.json').read_text());rows.append({'name':label,'exitCode':r.returncode,'device':capability['device_name'],'imageSha256':hashlib.sha256(image_path.read_bytes()).hexdigest(),'validationClean':True,'allocatorZero':True})
        assert np.array_equal(images['indexed-reference'],images['fixed-reference']),'Reference descriptor paths differ'
        difference=np.abs(images['indexed-reference'].astype('i4')-images['indexed-device'].astype('i4'));result.update(changedPixels=int(np.any(difference,axis=2).sum()),maximumByteChange=int(difference.max()))
        assert result['changedPixels']>0 and result['maximumByteChange']>0,'Public footprint positive control did not change the sampled mip color'
        assert hashlib.sha256(player.read_bytes()).hexdigest()==digest;result['status']='passed';print('PASS mip footprint',result['changedPixels'],result['maximumByteChange'],flush=True)
    finally:
        if result['status']=='running':result['status']='failed'
        save(output/'results.json',result)
if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',required=True,type=Path);parser.add_argument('--output',required=True,type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');args=parser.parse_args();run(args.executable.resolve(),args.output.resolve(),args.expected_validation)
