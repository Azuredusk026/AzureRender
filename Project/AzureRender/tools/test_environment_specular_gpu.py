"""Verify optional fitted environment response and material compatibility."""
import argparse
import base64
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

import numpy as np
from PIL import Image

from test_character_probe_gpu import prepare,linear
from test_cube_probe_gpu import observation_errors

BUDGET=.004


def fixture(root,case):
    project=prepare(root,{'name':'environment-specular','material':case['material']})
    path=root/'assets/surface.gltf';model=json.loads(path.read_text())
    material=model['materials'][0]
    material['pbrMetallicRoughness'].update(baseColorFactor=case['baseColor']+[1],
        metallicFactor=case['metallic'],roughnessFactor=case['roughness'])
    normal=np.array([np.sqrt(1-case['ndotv']**2),0,case['ndotv']])
    tangent=np.cross([0,1,0],normal);tangent/=np.linalg.norm(tangent)
    binary=bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',')[1]))
    struct.pack_into('<12f',binary,48,*(normal.tolist()*4))
    binary+=b'\0'*4+struct.pack('<16f',*((tangent.tolist()+[1])*4))
    model['buffers'][0]['byteLength']=len(binary)
    model['bufferViews'].append({'buffer':0,'byteOffset':112,'byteLength':64})
    model['accessors'].append({'bufferView':3,'componentType':5126,'count':4,'type':'VEC4'})
    model['meshes'][0]['primitives'][0]['attributes']['TANGENT']=3
    model['buffers'][0]['uri']='data:application/octet-stream;base64,'+base64.b64encode(binary).decode()
    if 'emissiveAlpha' in case:
        pixels=io.BytesIO();Image.new('RGBA',(1,1),(0,0,0,case['emissiveAlpha'])).save(pixels,format='PNG')
        model['images']=[{'uri':'data:image/png;base64,'+base64.b64encode(pixels.getvalue()).decode()}]
        model['textures']=[{'source':0}];material['emissiveTexture']={'index':0}
    path.write_text(json.dumps(model))
    radiance=struct.pack('<4e',.8,.4,.2,1)
    environment=b'AZENV001'+struct.pack('<III',2,1,1)+radiance*2
    (root/'assets/global.azenv').write_bytes(environment)
    (root/'assets/probe.azenv').write_bytes(environment)
    path=root/'assets/scene.azurelevel';level=json.loads(path.read_text())
    level['renderSettings']['skyIntensity']=int(case['source']=='global')
    if case['model'] is not None:level['renderSettings']['environmentSpecularModel']=case['model']
    if case['source']=='global':level.pop('reflectionProbe',None)
    path.write_text(json.dumps(level))
    return project


def run(player,output,reference,prepare_only,expected_validation):
    output.mkdir(parents=True,exist_ok=False)
    frozen=json.loads(reference.read_text())
    report={'schemaVersion':1,'status':'preparing','linearBudget':BUDGET,
        'scope':'Public fitted environment response; game-frame conformance remains independent.',
        'referenceSha256':hashlib.sha256(reference.read_bytes()).hexdigest(),
        'scriptSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'gpuExecuted':False,'cases':[],'comparisons':[],'failures':[]}
    if not prepare_only:
        report['playerSha256']=hashlib.sha256(player.read_bytes()).hexdigest()
        report['shaders']={path.name:hashlib.sha256(path.read_bytes()).hexdigest() for path in (player.parent/'shaders').glob('*.spv')}
    startup=None
    if hasattr(subprocess,'STARTUPINFO'):
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    images={}
    try:
        for backend in ('indexed','fixed'):
            for case in frozen['cases']:
                name=backend+'-'+case['name'];root=output/name
                project=fixture(root,case)
                inputs={path.relative_to(root).as_posix():hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in root.rglob('*') if path.is_file()}
                (root/'expected.json').write_text(json.dumps(case,indent=2))
                (root/'input-summary.json').write_text(json.dumps({'case':case['name'],'sha256':inputs,
                    'expectedSha256':hashlib.sha256((root/'expected.json').read_bytes()).hexdigest()},indent=2))
                expected_sha=hashlib.sha256((root/'expected.json').read_bytes()).hexdigest()
                item={'name':name,'inputs':inputs,'expectedSha256':expected_sha}
                report['cases'].append(item)
                if prepare_only:continue
                command=[str(player.resolve()),'--project',str(project.resolve()),
                    '--environment',str((root/'assets/global.azenv').resolve()),
                    '--width','128','--height','128','--capture-dir',str(root/'frames'),
                    '--capture-frames','1','--fixed-frame-step','--qa-camera','scene',
                    '--qa-light','scene','--qa-isolation','reflection','--runtime-report',str(root/'runtime.json')]
                if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                (root/'command.json').write_text(json.dumps(command,indent=2))
                result=subprocess.run(command,capture_output=True,text=True,errors='replace',timeout=120,startupinfo=startup)
                report['gpuExecuted']=True
                log=result.stdout+result.stderr;(root/'player.log').write_text(log,encoding='utf8')
                assert result.returncode==0,(name,'Player failure')
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name
                assert 'Allocator after unload: buffers=0 images=0' in log,name
                assert f'Character texture access: {backend}' in log,name
                validation='Validation layer: enabled' in log
                if expected_validation!='any':assert validation==(expected_validation=='enabled'),name
                runtime=json.loads((root/'runtime.json').read_text())
                assert not runtime['scriptErrors'] and not runtime['presentationErrors'],name
                assert inputs=={key:hashlib.sha256((root/key).read_bytes()).hexdigest()
                    for key in inputs},'Fixture changed during capture'
                assert expected_sha==hashlib.sha256((root/'expected.json').read_bytes()).hexdigest(),'Expected values changed during capture'
                path=root/'frames/frame_000000.png';image=np.asarray(Image.open(path).convert('RGB'));images[name]=image
                item.update(validationEnabled=validation,imageSha256=hashlib.sha256(path.read_bytes()).hexdigest())
                if case['expected'] is not None:
                    metrics=observation_errors(image[62:66,62:66],np.asarray(case['expected']))
                    item.update(metrics)
                    if metrics['maximumQuantizationIntervalLinearError']>BUDGET:report['failures'].append(name+': fitted response mismatch')
                else:
                    assert linear(image[62:66,62:66]).max()>.0005,(name,'character control is dark')
        if not prepare_only:
            pairs=[('legacy-omitted','legacy-explicit')]
            pairs += [(name+'-legacy',name+'-fit') for name in ('skin','face','fabric')]
            pairs += [(source+'-alpha-zero',source+'-alpha-full') for source in ('global','local')]
            for backend in ('indexed','fixed'):
                for left,right in pairs:
                    delta=int(abs(images[backend+'-'+left].astype(int)-images[backend+'-'+right].astype(int)).max())
                    report['comparisons'].append({'left':backend+'-'+left,'right':backend+'-'+right,'maximumByteDelta':delta,'budget':0})
                    assert delta==0,(backend,left,right,'compatibility or alpha coupling')
            backend_delta=max(int(abs(images[key].astype(int)-images[key.replace('indexed-','fixed-')].astype(int)).max()) for key in images if key.startswith('indexed-'))
            report['backendMaximumByteDelta']=backend_delta
            assert backend_delta<=1,'descriptor path mismatch'
            assert report['playerSha256']==hashlib.sha256(player.read_bytes()).hexdigest(),'Player changed during capture'
            assert report['shaders']=={path.name:hashlib.sha256(path.read_bytes()).hexdigest()
                for path in (player.parent/'shaders').glob('*.spv')},'Shaders changed during capture'
        assert report['referenceSha256']==hashlib.sha256(reference.read_bytes()).hexdigest(),'Reference changed during capture'
        assert report['scriptSha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'Script changed during capture'
        report['status']='prepared' if prepare_only else ('passed' if not report['failures'] else 'failed')
        assert not report['failures'],report['failures']
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:
        (output/'summary.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({'status':report['status'],'cases':len(report['cases']),'gpuExecuted':report['gpuExecuted']}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--reference',type=Path,default=Path(__file__).resolve().parents[1]/'tests/fixtures/environment_specular/gpu_reference.json')
    parser.add_argument('--prepare-only',action='store_true')
    parser.add_argument('--expected-validation',choices=('any','enabled','disabled'),default='any')
    args=parser.parse_args()
    if not args.prepare_only and not args.executable:parser.error('--executable is required for GPU execution')
    if args.output:
        run(args.executable,args.output,args.reference,args.prepare_only,args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='environment-specular-',dir=Path(__file__).resolve().parents[1]/'build/runs') as folder:
            run(args.executable,Path(folder)/'capture',args.reference,args.prepare_only,args.expected_validation)
