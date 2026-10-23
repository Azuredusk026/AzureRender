"""Exercise public view-scaled shells through Player with independent image controls."""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import tempfile
import uuid
import numpy as np
from PIL import Image
from native_render.light_lab import generate
from convert_material_semantics import complete_material_document


def sphere():
    vertices=[];normals=[];tangents=[];uv=[];indices=[]
    rings,segments=32,64
    for row in range(rings+1):
        phi=math.pi*row/rings
        for column in range(segments+1):
            theta=2*math.pi*column/segments
            n=[math.sin(phi)*math.cos(theta),math.cos(phi),math.sin(phi)*math.sin(theta)]
            vertices.append(n);normals.append(n);tangents.append([-math.sin(theta),0,math.cos(theta),1])
            uv.append([column/segments,row/rings])
    for row in range(rings):
        for column in range(segments):
            a=row*(segments+1)+column;b=a+1;c=a+segments+1;d=c+1
            indices.extend([a,b,c,b,d,c])
    model={'asset':{'version':'2.0'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'mesh':0}],
           'bufferViews':[],'accessors':[],'meshes':[{'primitives':[{'attributes':{},'material':0}]}]}
    payload=bytearray()
    def append(values,components,kind,dtype='<f4',component_type=5126):
        data=np.asarray(values,dtype=dtype);offset=len(payload);payload.extend(data.tobytes())
        index=len(model['accessors']);model['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=data.nbytes))
        accessor=dict(bufferView=index,componentType=component_type,count=len(values),type=kind)
        if kind=='VEC3':accessor.update(min=data.min(0).tolist(),max=data.max(0).tolist())
        model['accessors'].append(accessor);return index
    primitive=model['meshes'][0]['primitives'][0]
    for key,data,n in [('POSITION',vertices,3),('NORMAL',normals,3),('TANGENT',tangents,4),('TEXCOORD_0',uv,2)]:
        primitive['attributes'][key]=append(data,n,'VEC'+str(n))
    primitive['indices']=append(indices,1,'SCALAR','<u4',5125)
    return model,payload,vertices


def reference_bounds(positions,size,distance,fov,width):
    # Independent double-precision projection uses the mathematical half FOV;
    # the production shader's source-matching polynomial has a 1 px budget.
    p=np.asarray(positions,dtype=float);w=distance-p[:,2]
    half=math.radians(fov)/2;scale=1/math.tan(half)
    xy=p[:,:2]*[scale,-scale]
    xy/=np.maximum(np.linalg.norm(xy,axis=1,keepdims=True),1e-12)
    offset=xy*(math.pi/8/half)*width*np.clip(w*half*4.58366251,0,1)[:,None]*.005
    minimum=np.minimum(w,math.pi/2/half)/size
    offset=np.sign(offset)*np.maximum(np.abs(offset),minimum[:,None])
    pixels=((p[:,:2]*[scale,-scale]+offset)/w[:,None]+1)*size/2
    return np.array([pixels[:,0].min(),pixels[:,0].max()])


def run(player,output,selected=None,validation=None):
    names=['wide','zero','mask-zero','mask-half','fixed','document','uv1-zero','uv1-tilt','depth-only',
           'masked-clear','hidden','near','far','narrow-fov','resize','depth-positive','depth-negative',
           'two-assets','two-assets-fixed','uv1-mirror','fading','near-plane','morph','translated',
           'octahedral-forward','octahedral-back','color-only']
    if selected:
        assert set(selected)<=set(names);names=[name for name in names if name in selected]
    output.mkdir(parents=True,exist_ok=False);digest=hashlib.sha256(player.read_bytes()).hexdigest()
    frames={};rows=[];report=dict(schemaVersion=1,status='running',playerSha256=digest,captures=rows,
        scope='Public color, coverage and lifecycle controls; raw depth and native rim are separate acceptance evidence.')
    try:
        for name in names:
            print(name,flush=True);folder=output/name;generate(folder,'directional-near')
            model,payload,positions=sphere();primitive=model['meshes'][0]['primitives'][0]
            uv1=name.startswith(('uv1-','octahedral-'))
            if uv1:
                direction=[1,1] if name=='octahedral-back' else [.5,.5] if name in ['uv1-tilt','uv1-mirror'] else [0,0]
                values=np.tile(direction,(len(positions),1)).astype('<f4')
                offset=len(payload);payload.extend(values.tobytes());index=len(model['accessors'])
                model['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=values.nbytes))
                model['accessors'].append(dict(bufferView=index,componentType=5126,count=len(values),type='VEC2'))
                primitive['attributes']['TEXCOORD_1']=index
            if name=='morph':
                values=np.tile([.2,0,0],(len(positions),1)).astype('<f4');offset=len(payload);payload.extend(values.tobytes())
                index=len(model['accessors']);model['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=values.nbytes))
                model['accessors'].append(dict(bufferView=index,componentType=5126,count=len(values),type='VEC3',min=[.2,0,0],max=[.2,0,0]))
                primitive['targets']=[dict(POSITION=index)];model['meshes'][0]['weights']=[1];model['nodes'][0]['weights']=[1]
            model['buffers']=[dict(byteLength=len(payload),uri='data:application/octet-stream;base64,'+base64.b64encode(payload).decode())]
            mask=0 if name=='mask-zero' else 128 if name=='mask-half' else 255
            Image.new('RGBA',(4,4),(mask,255,0,255)).save(folder/'mask.png')
            Image.new('RGBA',(4,4),(160,160,160,0 if name=='masked-clear' else 255)).save(folder/'base.png')
            model['images']=[dict(uri='mask.png'),dict(uri='base.png')]
            model['textures']=[dict(source=0,sampler=0),dict(source=1,sampler=0)]
            model['samplers']=[dict(wrapS=33071,wrapT=33071,magFilter=9729,minFilter=9729)]
            shape=dict(schemaVersion=1,mode='view-scaled',width=0 if name=='zero' else 4,minimumPixels=.5,
                depthOffset=.1 if name=='depth-positive' else -.2 if name=='near-plane' else -.1 if name=='depth-negative' else 0,
                depthPrepass=name!='color-only',normalSource='tangent-octahedral-uv1' if name.startswith('octahedral-') else 'tangent-uv1' if uv1 else 'geometry',
                maskTexture=0,color=[1,0,0])
            material=dict(name='public-outline',alphaMode='MASK' if name=='masked-clear' else 'OPAQUE',
                pbrMetallicRoughness=dict(baseColorTexture=dict(index=1),metallicFactor=0,roughnessFactor=1),
                extras=dict(azureSlotId='outline',azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},
                    features=[] if name=='depth-only' else ['silhouette-outline'],outlineSurface=shape)))
            model['materials']=[material]
            if name in ['hidden','fading']:
                model['extensionsUsed']=['AZURE_node_opacity']
                model['nodes'][0]['extensions']={'AZURE_node_opacity':dict(schemaVersion=1,opacity=0 if name=='hidden' else .5)}
            (folder/'plane.gltf').write_text(json.dumps(model),encoding='utf-8')
            level=json.loads((folder/'level.azurelevel').read_text(encoding='utf-8'))
            camera=next(node for node in level['nodes'] if node['id']=='camera')['components']
            distance=1.05 if name=='near-plane' else 3 if name=='near' else 10 if name=='far' else 5
            camera['azure.transform']['data']['translation']=[0,0,distance]
            camera['azure.camera']['data']=dict(enabled=True,priority=1,fieldOfView=35 if name=='narrow-fov' else 55,nearPlane=.1,farPlane=20)
            level['lights'][0]['intensity']=1
            if name=='translated':
                next(node for node in level['nodes'] if node.get('resourceId')=='plane')['components']={
                    'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[.2,0,0]))}
            if name=='uv1-mirror':
                next(node for node in level['nodes'] if node.get('resourceId')=='plane')['components']={
                    'azure.transform':dict(type='azure.transform',version=1,data=dict(scale=[-1,1,1]))}
            if name.startswith('two-assets'):
                # The second asset owns its UV1 stream and distinct mask. A
                # different global material base exercises the real addresses.
                second=json.loads(json.dumps(model));blob=bytearray(payload)
                values=np.tile([.2,.4],(len(positions),1)).astype('<f4');offset=len(blob);blob.extend(values.tobytes())
                index=len(second['accessors']);second['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=values.nbytes))
                second['accessors'].append(dict(bufferView=index,componentType=5126,count=len(values),type='VEC2'))
                second['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_1']=index
                second['buffers']=[dict(byteLength=len(blob),uri='data:application/octet-stream;base64,'+base64.b64encode(blob).decode())]
                second['images'][0]['uri']='second-mask.png';Image.new('RGBA',(4,4),(128,255,0,255)).save(folder/'second-mask.png')
                second['materials'][0]['extras']['azureRenderMaterial']['outlineSurface'].update(normalSource='tangent-uv1',color=[0,1,0],width=3)
                (folder/'second.gltf').write_text(json.dumps(second),encoding='utf-8')
                level['resources'].append(dict(id='second',asset='assets:/second.gltf'))
                transform=lambda x: {'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[x,0,0],scale=[.6,.6,.6]))}
                next(node for node in level['nodes'] if node.get('resourceId')=='plane')['components']=transform(-1.1)
                level['nodes'].append(dict(id='second',resourceId='second',components=transform(1.1)))
            if name=='document':
                ids={str(i):str(uuid.uuid4()) for i in range(2)};material_id=str(uuid.uuid4())
                for i,item in enumerate(model['images']):
                    (folder/(item['uri']+'.azmeta')).write_text(json.dumps(dict(schemaVersion=1,id=ids[str(i)],dependencies=[],settings={})),encoding='utf-8')
                (folder/'outline.azurematerial').write_text(json.dumps(complete_material_document(material,model,ids)),encoding='utf-8')
                (folder/'outline.azurematerial.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=material_id,dependencies=list(ids.values()),settings={})),encoding='utf-8')
                next(node for node in level['nodes'] if node.get('resourceId')=='plane')['materialSlots']={'outline':material_id}
            (folder/'level.azurelevel').write_text(json.dumps(level),encoding='utf-8')
            size=512 if name=='resize' else 256
            command=[str(player),'--project',str(folder/'lab.azureproject'),'--width',str(size),'--height',str(size),
                '--capture-dir',str(folder/'frames'),'--capture-frames','2','--fixed-frame-step','--qa-camera','scene','--qa-light','scene',
                '--runtime-report',str(folder/'runtime.json'),'--set','render.features.showcase-platform=false','--set','render.features.face-sdf=false']
            if name in ['fixed','two-assets-fixed']:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            with (folder/'player.log').open('w',encoding='utf-8') as log:
                subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(folder/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
            assert 'Allocator after unload: buffers=0 images=0' in log,name
            if validation:assert ('Validation layer: enabled' in log)==(validation=='enabled'),name
            runtime=json.loads((folder/'runtime.json').read_text(encoding='utf-8'));assert not runtime['scriptErrors'] and not runtime['presentationErrors']
            manifest=json.loads((folder/'frames/capture_manifest.json').read_text(encoding='utf-8'))
            # The glTF loader appends its ordinary fallback material slot.
            materials=4 if name.startswith('two-assets') else 2
            coordinates=len(positions) if uv1 or name.startswith('two-assets') else 0
            assert manifest['outlineProjectionLayoutVersion']==1 and manifest['outlineProjectionMaterials']==materials
            assert manifest['outlineProjectionCoordinates']==coordinates
            assert manifest['outlineProjectionBytes']==16+materials*64+8*coordinates
            frame=folder/'frames/frame_000001.png';pixels=np.asarray(Image.open(frame).convert('RGB'));frames[name]=pixels
            assert np.array_equal(pixels,np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))),name
            red=(pixels[:,:,0]>240)&(pixels[:,:,1]<4)&(pixels[:,:,2]<4)
            count=int(red.sum());assert count>8 if name in ['wide','fixed','document','uv1-zero','uv1-tilt','near','far','narrow-fov','resize','depth-positive','depth-negative'] else True,name
            if name in ['zero','mask-zero','depth-only','masked-clear','hidden','octahedral-back']:assert count==0,(name,count)
            if name in ['two-assets','two-assets-fixed']:
                green=(pixels[:,:,1]>240)&(pixels[:,:,0]<4)&(pixels[:,:,2]<4)
                assert count>8 and green.sum()>8,(name,count,int(green.sum()))
            if name in ['fading','uv1-mirror']:assert count>8,name
            if name in ['masked-clear','hidden']:assert np.all(pixels==pixels[0,0]),name
            boundary_error=None
            if name in ['wide','fixed','document','uv1-zero','near','narrow-fov','resize']:
                columns=np.where(red)[1];measured=np.array([columns.min()+.5,columns.max()+.5])
                expected=reference_bounds(positions,size,distance,35 if name=='narrow-fov' else 55,4)
                boundary_error=float(np.abs(measured-expected).max())
                assert boundary_error<=1,(name,measured,expected,boundary_error)
            rows.append(dict(name=name,outlinePixels=count,playerSha256=digest,imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest(),
                validationEnabled='Validation layer: enabled' in log,outlineBytes=manifest['outlineProjectionBytes'],size=size,distance=distance,
                maximumHorizontalBoundaryErrorPixels=boundary_error))
        if 'wide' in frames:
            for name in ['fixed','document','uv1-zero','octahedral-forward','color-only']:
                if name in frames:assert np.array_equal(frames['wide'],frames[name]),name
            if 'uv1-tilt' in frames:assert np.any(frames['uv1-tilt']!=frames['wide'])
            if 'mask-half' in frames:
                counts={row['name']:row['outlinePixels'] for row in rows};assert 0<counts['mask-half']<counts['wide']
        if 'zero' in frames:
            for name in ['mask-zero','depth-only']:
                if name in frames:assert np.array_equal(frames['zero'],frames[name]),name
        if 'masked-clear' in frames and 'hidden' in frames:assert np.array_equal(frames['masked-clear'],frames['hidden'])
        if 'two-assets' in frames and 'two-assets-fixed' in frames:assert np.array_equal(frames['two-assets'],frames['two-assets-fixed'])
        if 'uv1-tilt' in frames and 'uv1-mirror' in frames:
            error=np.abs(frames['uv1-mirror'].astype(int)-frames['uv1-tilt'][:,::-1].astype(int))
            report['mirroredPixelsOverOneByte']=int(np.any(error>1,axis=2).sum())
            assert report['mirroredPixelsOverOneByte']<=4
        if 'fading' in frames and 'wide' in frames:
            counts={row['name']:row['outlinePixels'] for row in rows};assert 0<counts['fading']<counts['wide']
        if 'morph' in frames and 'translated' in frames:
            error=np.abs(frames['morph'].astype(int)-frames['translated'].astype(int))
            report['morphMaximumByteError']=int(error.max());assert error.max()<=1
        assert hashlib.sha256(player.read_bytes()).hexdigest()==digest
        report['status']='passed'
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:
        (output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--case',action='append')
    parser.add_argument('--expected-validation','--validation',dest='expected_validation',choices=['enabled','disabled'])
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.case,args.expected_validation)
    else:
        root=Path(__file__).resolve().parents[1]/'build/runs';root.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='outline_projection_',dir=root) as directory:
            run(args.executable.resolve(),Path(directory)/'captures',args.case,args.expected_validation)
