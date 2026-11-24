"""Verify lit transparent depth writing and equality using overlapping surfaces."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
from PIL import Image
from create_visibility_fixture import component, surface
from test_unlit_texture_layers_gpu import linear


def run(player, output, validation):
    output.mkdir(parents=True,exist_ok=False)
    inputs=[player,Path(__file__),Path(__file__).with_name('create_visibility_fixture.py'),
            Path(__file__).with_name('test_unlit_texture_layers_gpu.py'),*sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprint=lambda:{str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    frozen=fingerprint();records=[];images={}
    cases=['default','off','write-less','write-inclusive','coplanar-less','coplanar-inclusive']
    for fixed in [False,True]:
        for workflow in ['generic','fabric']:
            for case in cases:
                name=('fixed-' if fixed else 'indexed-')+workflow+'-'+case
                folder=output/name;assets=folder/'assets';assets.mkdir(parents=True)
                source=assets/'surfaces.gltf';surface(source,double_sided=True)
                model=json.loads(source.read_text())
                near=dict(name='Near',alphaMode='BLEND',doubleSided=True,
                    pbrMetallicRoughness=dict(baseColorFactor=[0,0,0,.5],metallicFactor=0,roughnessFactor=1),
                    emissiveFactor=[.4,0,0],extras=dict(azureRenderMaterial=dict(schemaVersion=2,
                        **{'class':workflow},features=['emissive-mask'])))
                if case!='default':near['extras']['azureRenderMaterial']['blendDepth']=dict(schemaVersion=1,
                    write=case!='off',compare='less-or-equal' if 'inclusive' in case else 'less')
                far=copy.deepcopy(near);far['name']='Far';far['emissiveFactor']=[0,.4,0]
                # The first transparent primitive is intentionally closer. The
                # second must fail its test when the first has written depth.
                rear=dict(name='Rear',doubleSided=True,pbrMetallicRoughness=dict(baseColorFactor=[.08,.08,.08,1]),
                    extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},
                        features=['unlit-surface'],surface=dict(schemaVersion=1,lighting='unlit'))))
                model['materials']=[near,far,rear]
                base=copy.deepcopy(model['meshes'][0])
                model['meshes']=[copy.deepcopy(base) for _ in range(3)]
                for i,mesh in enumerate(model['meshes']):mesh['primitives'][0]['material']=i
                model['nodes']=[dict(mesh=0,translation=[0,0,.2]),
                    dict(mesh=1,translation=[0,0,.2 if case.startswith('coplanar') else 0]),
                    dict(mesh=2,translation=[0,0,-.2])]
                model['scenes'][0]['nodes']=[0,1,2]
                source.write_text(json.dumps(model))
                level=dict(schemaVersion=1,id=name,sceneType='character',resources=[dict(id='surface',asset='assets:/surfaces.gltf')],
                    nodes=[dict(id='surface',resourceId='surface'),dict(id='camera',components={
                        'azure.transform':component('azure.transform',dict(translation=[0,0,3])),
                        'azure.camera':dict(type='azure.camera',version=2,data=dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=20))})],
                    lights=[],renderSettings=dict(schemaVersion=20,sceneLighting=True,skyIntensity=0,platform=False,background=False,
                        stylized=False,silhouetteOutline=False,innerOutline=False,bloomEnabled=False,antiAliasing=0,
                        exposure=0,toneMapping=False,contrast=1,saturation=1))
                (assets/'scene.azurelevel').write_text(json.dumps(level))
                project=folder/'project.azureproject';project.write_text(json.dumps(dict(schemaVersion=1,id=name,name=name,
                    mounts=[dict(name='assets',path='assets')],startupScene='assets:/scene.azurelevel')))
                command=[str(player),'--project',str(project),'--width','128','--height','128','--capture-dir',str(folder/'frames'),
                    '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene']
                if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                startup=None
                if os.name=='nt':
                    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
                result=subprocess.run(command,cwd=folder,startupinfo=startup,capture_output=True,timeout=120)
                log=result.stdout+result.stderr;(folder/'player.log').write_bytes(log);text=log.decode(errors='replace')
                assert result.returncode==0 and 'Allocator after unload: buffers=0 images=0' in text,name
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text),name
                if validation!='any':assert ('Validation layer: enabled' in text)==(validation=='enabled'),name
                image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))
                blocked=case in ['write-less','write-inclusive','coplanar-less']
                expected=np.array([.24,.04,.04]) if blocked else np.array([.12,.22,.02])
                error=float(np.max(abs(linear(image[64,64])-expected)))
                assert error<=.008,(name,error,image[64,64].tolist(),expected.tolist())
                if fixed:assert np.array_equal(image,images[workflow,case]),name
                else:images[workflow,case]=image
                if case=='off':assert np.array_equal(image,images[workflow,'default']),name
                records.append(dict(name=name,maximumLinearError=error,command=command));print(name,'passed',flush=True)
    assert fingerprint()==frozen
    (output/'report.json').write_text(json.dumps(dict(status='passed',frames=len(records),descriptorExact=True,
        cases=records,inputFingerprints=frozen),indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any')
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.expected_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='blend-depth-',dir=Path(__file__).resolve().parents[1]/'build/runs') as temp:
            run(args.executable.resolve(),Path(temp)/'check',args.expected_validation)
