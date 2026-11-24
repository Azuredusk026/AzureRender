"""Verify authored cube faces, LODs and projection through real Player draws.

PNG observations use the linear preimage of rounded sRGB8 samples. Decoded
center errors and output byte differences remain available as diagnostics.
"""
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

from test_character_probe_gpu import prepare, linear

COLORS=np.array([[.8,.1,.2],[.1,.8,.3],[.2,.3,.8],
                 [.7,.5,.1],[.4,.1,.7],[.1,.6,.7]])
NORMALS=[[1,0,1],[-1,0,1],[0,1,1],[0,-1,1],[0,0,1],[1,0,0],[0,0,1]]
FACES=['px','nx','py','ny','pz','nz','projected-origin']
BUDGET=.004
GRADIENT_PIXELS=[(48,51),(79,54),(52,78),(76,75),(63,63)]


def linear_byte_interval(image):
    """Linear preimage of each rounded sRGB8 byte, with display endpoints."""
    encoded=image.astype(np.float64)
    return linear(np.clip(encoded-.5,0,255)),linear(np.clip(encoded+.5,0,255))


def observation_errors(image,wanted):
    lower,upper=linear_byte_interval(image)
    interval=np.maximum(np.maximum(lower-wanted,wanted-upper),0)
    encoded=np.where(wanted<=.0031308,wanted*12.92,
        1.055*np.maximum(wanted,0)**(1/2.4)-.055)
    expected_bytes=np.rint(np.clip(encoded,0,1)*255).astype(np.int16)
    return {'maximumDecodedCenterLinearError':float(abs(linear(image)-wanted).max()),
        'maximumQuantizationIntervalLinearError':float(interval.max()),
        'maximumOutputByteDelta':int(abs(image.astype(np.int16)-expected_bytes).max())}


def shaded_normal(face):
    normal=np.asarray(NORMALS[face],float);normal/=np.linalg.norm(normal)
    tangent=np.cross([0,1,0],normal);tangent/=np.linalg.norm(tangent)
    bitangent=np.cross(normal,tangent)
    # The loader's standard flat RGBA8 normal is (128,128,255,255).
    result=normal+(tangent+bitangent)/255
    return result/np.linalg.norm(result),tangent


def cube_face(direction):
    x,y,z=direction;axis=int(np.argmax(np.abs(direction)))
    face=axis*2+int(direction[axis]<0)
    uv=[[-z,-y],[z,-y],[x,z],[x,-z],[x,-y],[-x,-y]][face]
    return face,(np.asarray(uv)/abs(direction[axis])+1)*.5


def cube_direction(face,u,v):
    return np.asarray([[1,-v,-u],[-1,-v,u],[u,1,v],
                       [u,-1,-v],[u,-v,1],[-u,-v,-1]][face])


def expected_pixels(face,roughness):
    # Independent Vulkan seamless-cube reference, including the 1x1 face
    # footprint. Adjacent face colors contribute away from the exact axis.
    result=np.zeros((4,4,3));normal,_=shaded_normal(face)
    for iy,y in enumerate(range(62,66)):
        for ix,x in enumerate(range(62,66)):
            position=np.array([(2*(x+.5)/128-1)*3*np.tan(np.deg2rad(55)/2),
                -(2*(y+.5)/128-1)*3*np.tan(np.deg2rad(55)/2),0])
            view=np.array([0,0,3])-position;view/=np.linalg.norm(view)
            direction=2*np.dot(view,normal)*normal-view
            if face==6:
                far=np.where(direction>0,4,-4)
                distance=np.min((far-position)[abs(direction)>1e-8]/direction[abs(direction)>1e-8])
                direction=position+distance*direction-[0,0,8]
            selected,uv=cube_face(direction)
            lod=roughness*2;lo=int(lod);fraction=lod-lo
            for mip,weight in ((lo,1-fraction),(min(lo+1,2),fraction)):
                side=4>>mip;sx,sy=uv*side-.5;ax,ay=np.floor([sx,sy]).astype(int)
                fx,fy=sx-ax,sy-ay
                for dx,wx in ((0,1-fx),(1,fx)):
                    for dy,wy in ((0,1-fy),(1,fy)):
                        tx,ty=ax+dx,ay+dy
                        neighbor=selected
                        if not (0<=tx<side and 0<=ty<side):
                            neighbor,_=cube_face(cube_direction(selected,2*(tx+.5)/side-1,2*(ty+.5)/side-1))
                        result[iy,ix]+=COLORS[neighbor]*(.25,.5,1)[mip]*weight*wx*wy
    return result


def fixture(root,face,roughness):
    project=prepare(root,{'name':'cube-probe','material':'pbr'})
    asset=root/'assets/surface.gltf'
    model=json.loads(asset.read_text())
    model['materials'][0]['pbrMetallicRoughness'].update(
        baseColorFactor=[1,1,1,1],metallicFactor=1,roughnessFactor=roughness)
    binary=bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',')[1]))
    normal=np.asarray(NORMALS[face],float);normal/=np.linalg.norm(normal)
    struct.pack_into('<12f',binary,48,*(normal.tolist()*4))
    _,tangent=shaded_normal(face)
    binary+=b'\0'*4+struct.pack('<16f',*((tangent.tolist()+[1])*4))
    model['buffers'][0]['byteLength']=len(binary)
    model['bufferViews'].append({'buffer':0,'byteOffset':112,'byteLength':64})
    model['accessors'].append({'bufferView':3,'componentType':5126,'count':4,'type':'VEC4'})
    model['meshes'][0]['primitives'][0]['attributes']['TANGENT']=3
    model['buffers'][0]['uri']='data:application/octet-stream;base64,'+base64.b64encode(binary).decode()
    asset.write_text(json.dumps(model))
    raw=bytearray(b'AZENV002'+struct.pack('<III',4,4,3))
    for mip,scale in enumerate((.25,.5,1)):
        size=4>>mip
        for color in COLORS:
            raw.extend(struct.pack('<4e',*(color*scale),1)*(size*size))
    (root/'assets/probe.azenv').write_bytes(raw)
    if face==6:
        path=root/'assets/scene.azurelevel';level=json.loads(path.read_text())
        level['reflectionProbe'].update(version=2,boxProjection=True,projectionOrigin=[0,0,8])
        path.write_text(json.dumps(level))
    return project


def case_definitions():
    cases=[{'name':f'{FACES[face]}-r{roughness}','face':face,'roughness':roughness}
           for roughness in (1,.5,.25) for face in range(7)]
    cases += [{'name':f'gradient-{FACES[face]}-r0.25','face':face,
               'roughness':.25,'gradient':True} for face in range(6)]
    cases.append({'name':'gradient-px-r0.25-rotated-z-90','face':0,
                  'roughness':.25,'gradient':True,'rotationDegrees':-90})
    return cases


def gradient_fixture(root,face,rotation_degrees=0):
    project=fixture(root,face,.25)
    raw=bytearray(b'AZENV002'+struct.pack('<III',4,4,3))
    for mip,scale in enumerate((.25,.5,1)):
        side=4>>mip
        for color in COLORS:
            for y in range(side):
                for x in range(side):
                    u,v=(x+.5)/side,(y+.5)/side
                    rgb=.04+color*.08+np.array([.55*u+.04*v,.03*u+.50*v,.22*u+.17*v])
                    raw.extend(struct.pack('<4e',*(rgb*scale),1))
    (root/'assets/probe.azenv').write_bytes(raw)
    if rotation_degrees:
        path=root/'assets/scene.azurelevel';level=json.loads(path.read_text())
        level['nodes'][0]['components']['azure.transform']['data']['rotation']=[0,0,rotation_degrees]
        path.write_text(json.dumps(level))
    return project


def read_cube_levels(path):
    # Decode the actual storage independently from the authored gradient.
    raw=path.read_bytes()
    assert raw[:8]==b'AZENV002','Expected a cube environment'
    width,height,count=struct.unpack_from('<III',raw,8)
    assert width==height and width>0 and count>0
    levels=[];offset=20
    for mip in range(count):
        side=max(1,width>>mip);values=6*side*side*4
        rgba=np.frombuffer(raw,dtype='<f2',count=values,offset=offset)
        levels.append(rgba.reshape(6,side,side,4)[...,:3].astype(np.float64))
        offset+=values*2
    assert offset==len(raw),'Unexpected cube payload length'
    return levels


def sample_cube_interior(direction,lod,levels):
    # These orientation cases deliberately keep both mip footprints inside
    # one face. The existing constant-face cases cover seamless edge taps.
    selected,uv=cube_face(direction)
    lo=int(np.floor(lod));fraction=lod-lo
    assert 0<=lo<len(levels)
    value=np.zeros(3);footprints=[]
    for mip,weight in ((lo,1-fraction),(min(lo+1,len(levels)-1),fraction)):
        if weight==0:continue
        face=levels[mip][selected];side=face.shape[0]
        sx,sy=uv*side-.5;ax,ay=np.floor([sx,sy]).astype(int)
        assert 0<=ax<side-1 and 0<=ay<side-1,'Gradient footprint reaches a cube seam'
        fx,fy=sx-ax,sy-ay
        sample=((1-fy)*((1-fx)*face[ay,ax]+fx*face[ay,ax+1])+
                fy*((1-fx)*face[ay+1,ax]+fx*face[ay+1,ax+1]))
        value+=sample*weight
        footprints.append({'mip':mip,'weight':weight,'side':side,
            'baseTexel':[int(ax),int(ay)],'fraction':[float(fx),float(fy)]})
    return value,{'face':selected,'uv':uv.tolist(),'footprints':footprints}


def gradient_expected(path,face,rotation_degrees=0):
    levels=read_cube_levels(path)
    # Scene Euler Z follows the engine's column-major transform convention.
    theta=np.deg2rad(rotation_degrees);c,s=np.cos(theta),np.sin(theta)
    rotation=np.array([[c,s,0],[-s,c,0],[0,0,1]])
    local_normal,tangent=shaded_normal(face)
    normal=rotation@local_normal
    result=[];samples=[]
    for x,y in GRADIENT_PIXELS:
        position=np.array([(2*(x+.5)/128-1)*3*np.tan(np.deg2rad(55)/2),
            -(2*(y+.5)/128-1)*3*np.tan(np.deg2rad(55)/2),0])
        view=np.array([0,0,3])-position;view/=np.linalg.norm(view)
        direction=2*np.dot(view,normal)*normal-view
        value,trace=sample_cube_interior(direction,.5,levels)
        result.append(value)
        samples.append({'pixel':[x,y],'worldPosition':position.tolist(),
            'direction':direction.tolist(),**trace})
    return np.asarray(result),{'rotationDegrees':[0,0,rotation_degrees],
        'worldShadedNormal':normal.tolist(),'worldTangent':(rotation@tangent).tolist(),
        'lod':.5,'samples':samples}


def case_expectation(root,case):
    inputs={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in
        ('test.azureproject','assets/surface.gltf','assets/scene.azurelevel','assets/probe.azenv')}
    if case.get('gradient'):
        wanted,sampling=gradient_expected(root/'assets/probe.azenv',case['face'],case.get('rotationDegrees',0))
    else:
        wanted=expected_pixels(case['face'],case['roughness'])
        sampling={'pixels':[[x,y] for y in range(62,66) for x in range(62,66)],
                  'lod':case['roughness']*2,'rotationDegrees':[0,0,0]}
    record={'schemaVersion':1,'case':case,'linearBudget':BUDGET,
        'reference':'software-cube-ray-bilinear-trilinear/1',
        'measurementModel':'sRGB8-quantization-preimage-linear-distance/1',
        'expectedLinearRgb':wanted.tolist(),'sampling':sampling,'inputs':inputs}
    (root/'expected.json').write_text(json.dumps(record,indent=2))
    (root/'input-summary.json').write_text(json.dumps({'schemaVersion':1,'case':case,
        'camera':{'position':[0,0,3],'fieldOfView':55,'resolution':[128,128]},
        'rendering':{'isolation':'reflection','metallic':1,'roughness':case['roughness'],
                     'toneMapping':False,'exposure':0},'sha256':inputs},indent=2))
    return record


def run(player,output,expected_validation='any',smoke=False):
    output.mkdir(parents=True,exist_ok=False)
    report={'schemaVersion':1,'status':'running','linearBudget':BUDGET,
        'measurementModel':'sRGB8-quantization-preimage-linear-distance/1',
        'playerSha256':hashlib.sha256(player.read_bytes()).hexdigest(),
        'scriptSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'shaders':{p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in (player.parent/'shaders').glob('*.spv')},
        'cases':[],'failures':[],
        'scope':'Public cube storage and sampling; native roughness/DFG parity is separate.'}
    pixels={}
    startup=None
    if hasattr(subprocess,'STARTUPINFO'):
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    try:
        for backend in ('indexed','fixed'):
            for case in case_definitions():
                name=f"{backend}-{case['name']}"
                root=output/name
                if case.get('gradient'):
                    project=gradient_fixture(root,case['face'],case.get('rotationDegrees',0))
                else:
                    project=fixture(root,case['face'],case['roughness'])
                expected=case_expectation(root,case)
                command=[str(player.resolve()),'--project',str(project.resolve()),
                    '--width','128','--height','128','--capture-dir',str(root/'frames'),
                    '--capture-frames','1','--fixed-frame-step','--qa-camera','scene',
                    '--qa-light','scene','--qa-isolation','reflection',
                    '--runtime-report',str(root/'runtime.json')]
                if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                (root/'command.json').write_text(json.dumps(command,indent=2))
                result=subprocess.run(command,capture_output=True,text=True,errors='replace',timeout=120,startupinfo=startup)
                log=result.stdout+result.stderr;(root/'player.log').write_text(log,encoding='utf8')
                assert result.returncode==0,(name,'Player failure; see player.log')
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
                assert 'Allocator after unload: buffers=0 images=0' in log,name
                assert f'Character texture access: {backend}' in log,name
                validation='Validation layer: enabled' in log
                if expected_validation!='any':assert validation==(expected_validation=='enabled'),name
                runtime=json.loads((root/'runtime.json').read_text())
                assert not runtime['scriptErrors'] and not runtime['presentationErrors'],name
                assert expected['inputs']=={key:hashlib.sha256((root/key).read_bytes()).hexdigest()
                    for key in expected['inputs']},'Cube fixture changed during capture'
                path=root/'frames/frame_000000.png'
                image=np.asarray(Image.open(path).convert('RGB'));pixels[name]=image
                if case.get('gradient'):
                    observed=np.array([image[y,x] for x,y in GRADIENT_PIXELS])
                else:
                    observed=image[62:66,62:66]
                value=linear(observed);wanted=np.asarray(expected['expectedLinearRgb'])
                metrics=observation_errors(observed,wanted)
                error=metrics['maximumQuantizationIntervalLinearError']
                report['cases'].append({'name':name,'case':case,
                    'expectedLinearRgb':wanted.reshape(-1,3).mean(axis=0).tolist(),
                    'meanLinearRgb':value.reshape(-1,3).mean(axis=0).tolist(),'maximumLinearError':error,
                    'inputs':expected['inputs'],'expectedSha256':hashlib.sha256((root/'expected.json').read_bytes()).hexdigest(),
                    **metrics,
                    'validationEnabled':validation,'imageSha256':hashlib.sha256(path.read_bytes()).hexdigest()})
                if error>BUDGET:report['failures'].append(name+': face or authored LOD mismatch')
                if smoke:
                    report['status']='smoke-passed' if not report['failures'] else 'failed'
                    assert not report['failures'],report['failures']
                    return
        report['backendMaximumByteDelta']=max(int(abs(pixels[k].astype(int)-pixels[k.replace('indexed-','fixed-')].astype(int)).max())
            for k in pixels if k.startswith('indexed-'))
        assert report['backendMaximumByteDelta']<=1,'descriptor output mismatch'
        assert report['playerSha256']==hashlib.sha256(player.read_bytes()).hexdigest(),'Player changed during capture'
        assert report['scriptSha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'Cube check changed during capture'
        assert report['shaders']=={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (player.parent/'shaders').glob('*.spv')},'Shaders changed during capture'
        assert not report['failures'],report['failures']
        report['status']='passed'
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:
        (output/'summary.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({'status':report['status'],'captures':len(report['cases']),
        'maximumLinearError':max(c['maximumLinearError'] for c in report['cases'])}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--expected-validation',choices=('any','enabled','disabled'),default='any')
    parser.add_argument('--smoke',action='store_true')
    args=parser.parse_args()
    if args.output:
        run(args.executable,args.output,args.expected_validation,args.smoke)
    else:
        with tempfile.TemporaryDirectory(prefix='cube-probe-',dir=Path(__file__).resolve().parents[1]/'build/runs') as folder:
            run(args.executable,Path(folder)/'capture',args.expected_validation,args.smoke)
