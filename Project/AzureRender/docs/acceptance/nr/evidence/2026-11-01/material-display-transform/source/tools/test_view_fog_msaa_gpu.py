"""Check view fog against analytic four-sample coverage and depth steps."""
import argparse
import base64
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import tempfile

import numpy as np
from PIL import Image
from create_visibility_fixture import component, surface
from test_unlit_texture_layers_gpu import linear


def run(player, output, validation, selected):
    output.mkdir(parents=True, exist_ok=False)
    files = [Path(__file__), player, Path(__file__).with_name('create_visibility_fixture.py'),
             Path(__file__).with_name('test_unlit_texture_layers_gpu.py'),
             *sorted((player.parent/'shaders').glob('*.spv'))]
    fingerprints = lambda: {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    frozen = fingerprints()
    records = []
    images = {}
    samples = np.array([[.375,.125],[.875,.375],[.125,.625],[.625,.875]])
    angle = math.radians(29)
    c, s = math.cos(angle), math.sin(angle)
    color, rear = np.array([.45,.25,.1]), np.array([.07,.09,.11])
    scatter, trans = np.array([.125,.03125,.25]), .5
    for fixed in [False, True]:
        for kind in selected or ['silhouette', 'depth-step', 'camera-mismatch', 'resize', 'single-sample']:
            size = 192 if kind == 'resize' else 128
            root = output/(('fixed-' if fixed else 'indexed-')+kind)
            assets = root/'assets'
            assets.mkdir(parents=True)
            surface(assets/'front.gltf', double_sided=True)
            model = json.loads((assets/'front.gltf').read_text())
            data = bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',')[1]))
            positions = []
            for i in range(4):
                x, y, z = struct.unpack_from('<3f', data, i*12)
                p = (c*x-s*y+.013, s*x+c*y+.029, z)
                positions.append(p)
                struct.pack_into('<3f', data, i*12, *p)
            model['buffers'][0]['uri'] = 'data:application/octet-stream;base64,'+base64.b64encode(data).decode()
            model['accessors'][0].update(min=np.min(positions,axis=0).tolist(), max=np.max(positions,axis=0).tolist())
            material = lambda rgb: dict(doubleSided=True, pbrMetallicRoughness=dict(baseColorFactor=[*rgb,1]),
                extras=dict(azureRenderMaterial=dict(schemaVersion=2, **{'class':'generic'},
                    features=['unlit-surface'], surface=dict(schemaVersion=1,lighting='unlit'))))
            model['materials'] = [material(color.tolist())]
            (assets/'front.gltf').write_text(json.dumps(model))
            surface(assets/'rear.gltf', double_sided=True)
            back = json.loads((assets/'rear.gltf').read_text())
            back['materials'] = [material(rear.tolist())]
            (assets/'rear.gltf').write_text(json.dumps(back))
            (assets/'view.azfog').write_bytes(b'AZFOG001'+struct.pack('<3I4e',1,1,1,*scatter,trans))
            view = dict(projection='perspective',position=[0,0,3],target=[0,0,2],up=[0,1,0],
                fieldOfViewRadians=math.radians(55),orthographicHeight=5,nearPlane=.1,farPlane=20,projectionOffset=[0,0])
            if kind == 'camera-mismatch': view['position'][0] = .25
            projection = dict(schemaVersion=1,extent=[size,size],pixelOrigin='upper-left',framePhase=0,
                pixelCenterOffset=.5,jitterRadius=0,depthEncoding=[0,1,1],minimumViewDepth=0,
                nearFadeWidth=10 if kind=='depth-step' else .000001)
            nodes = [dict(id='front',resourceId='front'),dict(id='camera',components={
                'azure.transform':component('azure.transform',dict(translation=[0,0,3])),
                'azure.camera':dict(type='azure.camera',version=2,data=dict(enabled=True,priority=1,
                    fieldOfView=55,nearPlane=.1,farPlane=20))})]
            if kind == 'depth-step':nodes.append(dict(id='rear',resourceId='rear',components={
                'azure.transform':component('azure.transform',dict(translation=[0,0,-3],scale=[10,10,1]))}))
            scene = dict(schemaVersion=1,id='msaa-fog',sceneType='character',nodes=nodes,lights=[],
                resources=[dict(id='front',asset='assets:/front.gltf'),dict(id='rear',asset='assets:/rear.gltf')],
                renderSettings=dict(schemaVersion=20,sceneLighting=True,skyIntensity=0,platform=False,background=False,
                    stylized=False,silhouetteOutline=False,innerOutline=False,bloomEnabled=False,
                    antiAliasing=0 if kind=='single-sample' else 4,
                    exposure=0,toneMapping=False,contrast=1,saturation=1))
            variants = {}
            for enabled in [False, True]:
                variant = 'on' if enabled else 'off'
                level = copy.deepcopy(scene)
                level['viewFog'] = dict(schemaVersion=1,asset='assets:/view.azfog',enabled=enabled,
                    projection=projection,captureView=view)
                (assets/(variant+'.azurelevel')).write_text(json.dumps(level))
                project = root/(variant+'.azureproject')
                project.write_text(json.dumps(dict(schemaVersion=1,id='msaa-fog',name='MSAA fog coverage',
                    mounts=[dict(name='assets',path='assets')],startupScene='assets:/'+variant+'.azurelevel')))
                frames = root/variant
                command = [str(player),'--project',str(project.resolve()),'--width',str(size),'--height',str(size),
                    '--qa-camera','scene','--qa-light','scene','--fixed-frame-step','--smoke-frames','1',
                    '--capture-dir',str(frames.resolve()),'--capture-frames','1']
                if fixed:command += ['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                result = subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
                (root/(variant+'.log')).write_text(result.stdout)
                assert result.returncode == 0, (kind,variant,result.stdout[-2000:])
                assert 'Allocator after unload: buffers=0 images=0' in result.stdout
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',result.stdout)
                if validation != 'any':assert ('Validation layer: enabled' in result.stdout)==(validation=='enabled')
                manifest = json.loads((frames/'capture_manifest.json').read_text())
                sample_count = 1 if kind=='single-sample' else 4
                assert manifest['rasterizationSamples']==sample_count and manifest['snapshotRasterizationSamples']==sample_count
                assert manifest['multisampleFallback']==''
                expected_match = 'disabled' if not enabled else 'camera-mismatch' if kind=='camera-mismatch' else 'matched'
                assert manifest['viewFogMatch']==expected_match
                variants[variant] = np.asarray(Image.open(frames/'frame_000000.png').convert('RGB'))
            yy, xx = np.mgrid[0:size,0:size]
            positions = np.array([[.5,.5]]) if kind=='single-sample' else samples
            x = ((xx[...,None]+positions[:,0])/size*2-1)*3*math.tan(math.radians(55)/2)-.013
            y = (1-(yy[...,None]+positions[:,1])/size*2)*3*math.tan(math.radians(55)/2)-.029
            covered = (abs(c*x+s*y)<.5)&(abs(-s*x+c*y)<.5)
            counts = covered.sum(axis=2)*(4 if kind=='single-sample' else 1)
            # Vertex snapping has device subpixel precision. Exclude only samples
            # within two 1/256-pixel units of the analytic polygon boundary.
            local_x, local_y = abs(c*x+s*y), abs(-s*x+c*y)
            margin = 2/256*2*3*math.tan(math.radians(55)/2)/size*math.sqrt(2)
            uncertain = (((abs(local_x-.5)<margin)&(local_y<=.5+margin))|
                         ((abs(local_y-.5)<margin)&(local_x<=.5+margin))).any(axis=2)
            valid = ~uncertain
            assert uncertain.sum()<=size/4,'Rasterization ambiguity must stay local'
            if kind!='single-sample':
                assert all(np.count_nonzero(counts==n)>8 for n in [1,2,3]),'Fixture must exercise all partial coverages'
            background = rear if kind=='depth-step' else linear(variants['off'][0,0])
            k = counts[...,None]/4
            baseline = color*k+background*(1-k)
            baseline_error = float(abs(linear(variants['off'])-baseline)[valid].max())
            assert baseline_error<=.008,('analytic-coverage',kind,baseline_error)
            def fog(rgb,depth):
                weight = min(depth/projection['nearFadeWidth'],1)
                return rgb*(1+(trans-1)*weight)+scatter*weight
            expected = baseline if kind=='camera-mismatch' else fog(color,3)*k+(
                fog(rear,6) if kind=='depth-step' else background)*(1-k)
            error = abs(linear(variants['on'])-expected)
            maximum = float(error[valid].max())
            assert maximum<=.008,('per-sample-fog',kind,maximum,
                {str(n):float(error[(counts==n)&valid].max()) for n in range(5) if np.any((counts==n)&valid)})
            for variant,pixels in variants.items():
                if fixed:assert np.array_equal(pixels,images[(kind,variant)]),(kind,variant)
                else:images[(kind,variant)] = pixels
            records.append(dict(case=kind,fixed=fixed,size=size,maximumLinearError=maximum,
                baselineMaximumLinearError=baseline_error,coverageCounts={str(n):int(np.sum(counts==n)) for n in range(5)}))
            records[-1]['ambiguousRasterizationPixels'] = int(uncertain.sum())
            print(kind,'fixed' if fixed else 'indexed','passed',flush=True)
    assert fingerprints()==frozen,'Test inputs changed during execution'
    report = dict(status='passed',cases=records,maximumLinearError=max(r['maximumLinearError'] for r in records),
        budget=.008,descriptorExact=True,inputFingerprints=frozen)
    (output/'report.json').write_text(json.dumps(report,indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any')
    parser.add_argument('--case',choices=['silhouette','depth-step','camera-mismatch','resize','single-sample'],action='append')
    args = parser.parse_args()
    if args.output:
        run(args.executable.resolve(),args.output.resolve(),args.expected_validation,args.case)
    else:
        with tempfile.TemporaryDirectory(prefix='view-fog-msaa-',dir=Path(__file__).resolve().parents[1]/'build/runs') as directory:
            run(args.executable.resolve(),Path(directory)/'check',args.expected_validation,args.case)
