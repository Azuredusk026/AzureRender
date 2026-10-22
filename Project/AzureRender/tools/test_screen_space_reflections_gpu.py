"""Real Player SSR controls with independent planar-mirror projection.

--suite core: 10 short Player launches across indexed/fixed descriptors.
--suite extended: core plus live glTF motion, fallback, culling and occlusion.
--prepare-only writes public projects and commands without starting Player.
"""
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
import sys
import time
import tempfile

import numpy as np
from PIL import Image, ImageDraw

WIDTH, HEIGHT = 320, 240
CAMERA = np.array([0., 2.2, 5.])
PITCH = math.atan2(CAMERA[1], CAMERA[2])
FORWARD = np.array([0., -math.sin(PITCH), -math.cos(PITCH)])
UP = np.array([0., math.cos(PITCH), -math.sin(PITCH)])
FOV = math.radians(55.)
LEFT, RIGHT = -.7, .7
TARGET_Y, HALF = 1., .4
SOURCE_RGB = [.8, .04, .015]
# Broad engineering thresholds; geometric alignment and color are both required.
BUDGETS = dict(minimumRedSignal=.003, minimumActivePixels=12,
               redPixelThreshold=.005, minimumEnergyInsideExpected=.65,
               maximumCentroidErrorPixels=9., maximumCompatibilityByteError=2,
               maximumBeautyIsolationMeanLinearError=.015,
               maximumSuppressedSignalRatio=.25)
SSR = dict(schemaVersion=1, enabled=True, maxDistance=12., thickness=.15,
           normalBias=.03, maxSteps=128, maxRoughness=.7, edgeFade=.08, strength=1.)


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2)+'\n', encoding='utf-8')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def component(kind, data, version=1):
    return dict(type=kind, version=version, data=data)


def project(points):
    """Pinhole camera geometry, independent of renderer matrices and SSR marching."""
    relative = np.asarray(points, dtype=float)-CAMERA
    depth = relative @ FORWARD
    assert np.all(depth > .1), 'oracle points must be in front of camera'
    focal = HEIGHT/(2*math.tan(FOV/2))
    return np.column_stack((WIDTH/2+focal*relative[:, 0]/depth,
                            HEIGHT/2-focal*(relative @ UP)/depth))


def target_corners(x):
    return np.array([[x-HALF, TARGET_Y-HALF, 0], [x+HALF, TARGET_Y-HALF, 0],
                     [x+HALF, TARGET_Y+HALF, 0], [x-HALF, TARGET_Y+HALF, 0]])


def oracle(x):
    physical = target_corners(x)
    virtual = physical.copy(); virtual[:, 1] *= -1
    # Verify the virtual-image rays intersect the actual finite floor.
    fractions = CAMERA[1]/(CAMERA[1]-virtual[:, 1])
    floor_hits = CAMERA+(virtual-CAMERA)*fractions[:, None]
    assert np.max(abs(floor_hits[:, 0])) < 4 and np.max(abs(floor_hits[:, 2])) < 4
    return dict(sourcePolygon=project(physical).tolist(),
                reflectionPolygon=project(virtual).tolist(),
                reflectionCenter=project([[x, -TARGET_Y, 0]])[0].tolist(),
                floorIntersections=floor_hits.tolist())


def polygon_mask(polygon, shrink=1.):
    points = np.asarray(polygon, dtype=float)
    center = points.mean(axis=0); points = center+(points-center)*shrink
    image = Image.new('L', (WIDTH, HEIGHT)); ImageDraw.Draw(image).polygon(
        [tuple(point) for point in points], fill=255)
    return np.asarray(image)>0


def bounds_mask(polygon, padding=5):
    points=np.asarray(polygon); minimum=np.floor(points.min(axis=0)-padding).astype(int)
    maximum=np.ceil(points.max(axis=0)+padding).astype(int)
    mask=np.zeros((HEIGHT, WIDTH), bool)
    mask[max(0,minimum[1]):min(HEIGHT,maximum[1]+1),
         max(0,minimum[0]):min(WIDTH,maximum[0]+1)]=True
    return mask


def linear(image):
    encoded=np.asarray(image, dtype=float)/255
    return np.where(encoded<=.04045,encoded/12.92,((encoded+.055)/1.055)**2.4)


def red_signal(on, off):
    delta=linear(on)-linear(off)
    return np.maximum(delta[...,0]-.5*(np.maximum(delta[...,1],0)+np.maximum(delta[...,2],0)),0)


def measure_reflection(on, off, expected, name):
    mask=polygon_mask(expected['reflectionPolygon'], .7)
    wide=bounds_mask(expected['reflectionPolygon'])
    signal=red_signal(on,off)
    mean=float(signal[mask].mean()) if mask.any() else 0.
    active=int(np.count_nonzero((signal>BUDGETS['redPixelThreshold'])&mask))
    weight=np.where(wide,signal,0); total=float(weight.sum())
    ys,xs=np.indices(signal.shape)
    centroid=np.array([float((weight*xs).sum()/max(total,1e-12)),
                       float((weight*ys).sum()/max(total,1e-12))])
    error=float(np.linalg.norm(centroid-expected['reflectionCenter']))
    # The whole screen comparison is valid only for reflection isolation:
    # source emissive is absent from the displayed image under this isolation.
    concentration=float(signal[wide].sum()/max(float(signal.sum()),1e-12))
    result=dict(name=name,meanRedSignal=mean,activePixels=active,
                weightedCentroid=centroid.tolist(),centroidErrorPixels=error,
                energyInsideExpected=concentration)
    assert mean>BUDGETS['minimumRedSignal'],(name,'missing colored reflection',result)
    assert active>=BUDGETS['minimumActivePixels'],(name,'reflection lacks area',result)
    assert error<=BUDGETS['maximumCentroidErrorPixels'],(name,'reflection position disagrees with mirror geometry',result)
    assert concentration>=BUDGETS['minimumEnergyInsideExpected'],(name,'signal is outside reflected geometry',result)
    return result


def compatible(left,right,mask,name):
    delta=np.abs(left.astype(int)-right.astype(int))
    maximum=int(delta[mask].max())
    assert maximum<=BUDGETS['maximumCompatibilityByteError'],(name,maximum)
    return dict(name=name,maximumByteError=maximum)


def gltf(path,positions,normals,material,reverse=False,motion=False):
    indices=[0,2,1,0,3,2] if reverse else [0,1,2,0,2,3]
    positions=np.asarray(positions,dtype=float)
    binary=struct.pack('<24f6H',*positions.flatten(),*(list(normals)*4),*indices)
    document=dict(asset=dict(version='2.0',generator='Azure SSR public fixture',extras=dict(license='CC0-1.0')),
        scene=0,scenes=[dict(nodes=[0])],nodes=[dict(mesh=0)],
        buffers=[],bufferViews=[dict(buffer=0,byteOffset=0,byteLength=48),
            dict(buffer=0,byteOffset=48,byteLength=48),dict(buffer=0,byteOffset=96,byteLength=12)],
        accessors=[dict(bufferView=0,componentType=5126,count=4,type='VEC3',min=positions.min(axis=0).tolist(),max=positions.max(axis=0).tolist()),
            dict(bufferView=1,componentType=5126,count=4,type='VEC3'),dict(bufferView=2,componentType=5123,count=6,type='SCALAR')],
        materials=[material],meshes=[dict(primitives=[dict(attributes=dict(POSITION=0,NORMAL=1),indices=2,material=0,mode=4)])])
    if motion:
        binary+=struct.pack('<3f9f',0,.25,.5,LEFT,0,0,RIGHT,0,0,RIGHT,0,0)
        document['bufferViews'] += [dict(buffer=0,byteOffset=108,byteLength=12),dict(buffer=0,byteOffset=120,byteLength=36)]
        document['accessors'] += [dict(bufferView=3,componentType=5126,count=3,type='SCALAR',min=[0],max=[.5]),
            dict(bufferView=4,componentType=5126,count=3,type='VEC3')]
        document['animations']=[dict(name='move',samplers=[dict(input=3,output=4,interpolation='STEP')],
            channels=[dict(sampler=0,target=dict(node=0,path='translation'))])]
    document['buffers']=[dict(byteLength=len(binary),uri='data:application/octet-stream;base64,'+base64.b64encode(binary).decode())]
    write_json(path,document)


def cases(suite):
    result=[dict(name='on'),dict(name='off',enabled=False),dict(name='default',omitted=True),
            dict(name='hidden',hidden=True),dict(name='beauty',isolation='beauty')]
    if suite=='extended':
        result += [dict(name='motion',motion=True,frames=3),dict(name='rough',roughness=.9),
            dict(name='rough-off',roughness=.9,enabled=False),dict(name='short-ray',maxDistance=.1),
            dict(name='disabled-specular',disabled=True),dict(name='occluded',occluder=True),
            dict(name='back-single',reverse=True),dict(name='back-double',reverse=True,double=True),
            dict(name='mirrored',mirrored=True),dict(name='offscreen',x=4.1),
            dict(name='offscreen-off',x=4.,enabled=False),dict(name='edge-hard',x=2.6,edgeFade=.001),
            dict(name='edge-fade',x=2.6,edgeFade=.5),dict(name='reload'),
            dict(name='lit',lit=True),dict(name='lit-off',lit=True,enabled=False),
            dict(name='lit-beauty',lit=True,isolation='beauty')]
    return result


def prepare(root,case):
    assets=root/'assets';assets.mkdir(parents=True,exist_ok=False)
    material=dict(doubleSided=False,pbrMetallicRoughness=dict(baseColorFactor=[.7,.7,.7,1],
                  metallicFactor=1,roughnessFactor=case.get('roughness',.12)))
    gltf(assets/'floor.gltf',[[-4,0,4],[4,0,4],[4,0,-4],[-4,0,-4]],[0,1,0],material)
    target=dict(doubleSided=case.get('double',False),emissiveFactor=[0,0,0] if case.get('lit') else SOURCE_RGB,
                pbrMetallicRoughness=dict(baseColorFactor=[*SOURCE_RGB,1] if case.get('lit') else [0,0,0,1],metallicFactor=0,roughnessFactor=1))
    gltf(assets/'target.gltf',[[-HALF,-HALF,0],[HALF,-HALF,0],[HALF,HALF,0],[-HALF,HALF,0]],
         [0,0,-1 if case.get('reverse') else 1],target,case.get('reverse',False),case.get('motion',False))
    settings=dict(sceneLighting=True,environmentSpecularModel='ggx-multiscatter-fit',skyIntensity=0,
        skyMode=2,skyColor=[0,0,0],cameraNear=.1,cameraFar=30,shadowDistance=30,platform=False,
        stylized=False,silhouetteOutline=False,innerOutline=False,bloomEnabled=False,antiAliasing=0,
        exposure=0,toneMapping=False,contrast=1,saturation=1)
    if not case.get('omitted'):
        configuration=copy.deepcopy(SSR);configuration['enabled']=case.get('enabled',True)
        for field in ('maxDistance','edgeFade'):
            if field in case:configuration[field]=case[field]
        settings['screenSpaceReflection']=configuration
    x=case.get('x',LEFT)
    nodes=[dict(id='floor',resourceId='floor'),dict(id='target',resourceId='target',components={
        'azure.transform':component('azure.transform',dict(translation=[0 if case.get('motion') else x,TARGET_Y,0],scale=[-1 if case.get('mirrored') else 1,1,1])),
        'azure.renderable':component('azure.renderable',dict(visible=not case.get('hidden',False)))}),
        dict(id='camera',components={'azure.transform':component('azure.transform',dict(translation=CAMERA.tolist(),rotation=[math.degrees(PITCH),0,0])),
            'azure.camera':component('azure.camera',dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=30))})]
    if case.get('motion'):
        nodes[1]['components']['azure.animator']=component('azure.animator',dict(asset='assets:/motion.json',state='move',startTime=0),2)
        write_json(assets/'motion.json',dict(schemaVersion=1,initial='move',states=[dict(name='move',clip=0,loop=False)],transitions=[]))
    resources=[dict(id=key,asset='assets:/'+key+'.gltf') for key in ('floor','target')]
    if case.get('occluder'):
        gltf(assets/'occluder.gltf',[[-.5,-.5,0],[.5,-.5,0],[.5,.5,0],[-.5,.5,0]],[0,0,1],
            dict(doubleSided=True,pbrMetallicRoughness=dict(baseColorFactor=[0,0,0,1],metallicFactor=0,roughnessFactor=1)))
        resources.append(dict(id='occluder',asset='assets:/occluder.gltf'))
        nodes.append(dict(id='occluder',resourceId='occluder',components={'azure.transform':component('azure.transform',dict(translation=[-.59,.5,.78],scale=[1,.8,1]))}))
    texel=struct.pack('<4e',.02,.03,.05,1)
    (assets/'probe.azenv').write_bytes(b'AZENV001'+struct.pack('<III',2,1,1)+texel*2)
    lights=[]
    if case.get('lit'):
        nodes.append(dict(id='light',components={'azure.transform':component('azure.transform',dict(translation=[LEFT,1,2]))}))
        lights=[dict(id='source-light',nodeId='light',color=[1,1,1],intensity=8,radius=6,enabled=True)]
    level=dict(schemaVersion=1,id='ssr-'+case['name'],sceneType='character',resources=resources,nodes=nodes,lights=lights,renderSettings=settings,
        reflectionProbe=dict(version=1,texture='assets:/probe.azenv',center=[0,0,0],halfExtents=[20,20,20],capturePosition=[0,0,0],intensity=1,blendDistance=0,boxProjection=False))
    write_json(assets/'scene.azurelevel',level)
    project=root/'project.azureproject'
    write_json(project,dict(schemaVersion=1,id='ssr-fixture',name='Screen reflection contract',mounts=[dict(name='assets',path='assets')],startupScene='assets:/scene.azurelevel'))
    expected=oracle(x);expected['motionOracles']=[oracle(LEFT),oracle(RIGHT)] if case.get('motion') else []
    write_json(root/'expected.json',expected)
    return project,expected


def capture(player,root,backend,case,prepare_only,expected_validation):
    project,expected=prepare(root,case)
    command=[str(player.resolve()) if player else '<PLAYER>','--project',str(project.resolve()),
        '--width',str(WIDTH),'--height',str(HEIGHT),'--capture-dir',str(root/'frames'),
        '--capture-frames',str(case.get('frames',1)),'--capture-fps','4','--fixed-frame-step',
        '--qa-camera','scene','--qa-light','scene','--qa-isolation',case.get('isolation','reflection'),
        '--runtime-report',str(root/'runtime.json')]
    if case.get('motion'):command+=['--qa-animation']
    if case.get('disabled'):command+=['--qa-effect','specular','--qa-effect-state','disabled']
    if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
    write_json(root/'command.json',command)
    inputs={p.relative_to(root).as_posix():sha(p) for p in root.rglob('*') if p.is_file()}
    record=dict(name=backend+'-'+case['name'],case=case,inputSha256=inputs,expected=expected,gpuExecuted=False)
    if prepare_only:return None,record
    startup=None
    if hasattr(subprocess,'STARTUPINFO'):
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    before=time.monotonic()
    process=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120,startupinfo=startup)
    log=process.stdout+process.stderr;(root/'player.log').write_text(log,encoding='utf-8')
    record.update(gpuExecuted=True,exitCode=process.returncode,elapsedSeconds=time.monotonic()-before)
    assert process.returncode==0,(record['name'],'Player failure; see player.log')
    assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[validation warning\]|\[error\]',log,re.I),record['name']
    assert f'Character texture access: {backend}' in log,(record['name'],'descriptor path mismatch')
    assert 'Allocator after unload: buffers=0 images=0' in log,(record['name'],'unload leak')
    validation='Validation layer: enabled' in log;record['validationEnabled']=validation
    if expected_validation!='any':assert validation==(expected_validation=='enabled'),record['name']
    runtime=json.loads((root/'runtime.json').read_text(encoding='utf-8'))
    assert not runtime['scriptErrors'] and not runtime['presentationErrors'],record['name']
    manifest=json.loads((root/'frames/capture_manifest.json').read_text(encoding='utf-8'))
    assert manifest['qaCamera']=='scene' and manifest['qaLight']=='scene',record['name']
    assert manifest['qaIsolation']==case.get('isolation','reflection'),record['name']
    assert np.allclose(manifest['qaCameraPosition'],CAMERA,atol=1e-5),(record['name'],'camera pose differs from independent oracle')
    assert np.allclose(manifest['qaCameraTarget'],CAMERA+FORWARD,atol=1e-5),(record['name'],'camera direction differs from independent oracle')
    assert manifest['sceneRenderWidth']==WIDTH and manifest['sceneRenderHeight']==HEIGHT
    assert manifest['antiAliasing']==0
    paths=sorted((root/'frames').glob('frame_*.png'))
    assert len(paths)==case.get('frames',1),record['name']
    images=[np.asarray(Image.open(p).convert('RGB')) for p in paths]
    assert all(image.shape==(HEIGHT,WIDTH,3) for image in images)
    assert all(sha(root/key)==value for key,value in inputs.items()),'fixture mutated during capture'
    record['imageSha256']={p.name:sha(p) for p in paths}
    record['allocatorAfterUnload']=dict(buffers=0,images=0)
    if case.get('motion'):
        animations={v['node']:v for v in runtime['animations']}
        assert 'target' in animations and animations['target']['time']>=.25,(record['name'],'animation clock did not advance')
        record['animation']=animations['target']
    return images,record


def evaluate(frames,suite):
    metrics=[]
    for backend in ('indexed','fixed'):
        images=frames[backend];on,off=images['on'][0],images['off'][0]
        expected=oracle(LEFT);mask=polygon_mask(expected['reflectionPolygon'],.7)
        positive=measure_reflection(on,off,expected,backend+'-visible-target');metrics.append(positive)
        for key in ('default','hidden'):
            metrics.append(compatible(images[key][0],off,mask,backend+'-'+key))
        source_mask=polygon_mask(expected['sourcePolygon'],.6)
        source=linear(images['beauty'][0])[source_mask].mean(axis=0)
        assert source[0]>.15 and source[0]>3*max(source[1],source[2]),(backend,'colored source control is absent',source.tolist())
        isolated_source=linear(on)[source_mask].mean(axis=0)
        assert isolated_source[0]<.03,(backend,'reflection isolation unexpectedly retains source emissive')
        difference=float(abs(linear(images['beauty'][0])-linear(on))[mask].mean())
        assert difference<BUDGETS['maximumBeautyIsolationMeanLinearError'],(backend,'snapshot radiance is clipped by isolation',difference)
        metrics.append(dict(name=backend+'-full-source-under-reflection-isolation',sourceBeautyLinear=source.tolist(),sourceReflectionLinear=isolated_source.tolist(),receiverBeautyIsolationMeanLinearError=difference))
        if suite!='extended':continue
        lit=measure_reflection(images['lit'][0],images['lit-off'][0],expected,backend+'-lit-source');metrics.append(lit)
        lit_source=linear(images['lit-beauty'][0])[source_mask].mean(axis=0)
        assert lit_source[0]>.05 and lit_source[0]>2*max(lit_source[1],lit_source[2]),(backend,'point light source control is absent',lit_source.tolist())
        metrics.append(dict(name=backend+'-lit-source-control',sourceLinear=lit_source.tolist(),sourceEmissiveFactor=[0,0,0]))
        for key,baseline in [('rough','rough-off'),('short-ray','off'),('back-single','off'),('offscreen','offscreen-off'),('reload','on')]:
            case_oracle=oracle(4.1 if key=='offscreen' else LEFT)
            metrics.append(compatible(images[key][0],images[baseline][0],polygon_mask(case_oracle['reflectionPolygon'],.7),backend+'-'+key))
        for key in ('back-double','mirrored'):
            metrics.append(measure_reflection(images[key][0],off,expected,backend+'-'+key))
        disabled=float(linear(images['disabled-specular'][0])[mask].max())
        assert disabled<.002,(backend,'specular disable leaks reflection',disabled)
        occluded=float(red_signal(images['occluded'][0],off)[mask].mean())
        assert occluded<positive['meanRedSignal']*BUDGETS['maximumSuppressedSignalRatio'],(backend,'occlusion leaks reflected source',occluded)
        metrics.append(dict(name=backend+'-occlusion',meanRedSignal=occluded,unoccluded=positive['meanRedSignal']))
        motion=[]
        # t=0 and t=.5 are captured by the same Player process with three
        # deterministic 4 Hz frames. Runtime animation clocks are also checked.
        for index,x in [(0,LEFT),(2,RIGHT)]:
            motion.append(measure_reflection(images['motion'][index],off,oracle(x),backend+'-motion-'+str(index)))
        observed=np.array(motion[1]['weightedCentroid'])-motion[0]['weightedCentroid']
        wanted=np.array(oracle(RIGHT)['reflectionCenter'])-oracle(LEFT)['reflectionCenter']
        assert observed[0]>20 and np.linalg.norm(observed-wanted)<12,(backend,'reflected motion disagrees with geometry',observed.tolist(),wanted.tolist())
        metrics.extend(motion);metrics.append(dict(name=backend+'-same-process-motion',observedPixels=observed.tolist(),expectedPixels=wanted.tolist(),frames=3))
        edge_mask=polygon_mask(oracle(2.6)['reflectionPolygon'],.7)
        hard=float(red_signal(images['edge-hard'][0],off)[edge_mask].mean())
        fade=float(red_signal(images['edge-fade'][0],off)[edge_mask].mean())
        assert hard>.003 and 0<=fade<hard*.8,(backend,'screen-edge confidence does not attenuate',hard,fade)
        metrics.append(dict(name=backend+'-edge-confidence',hardSignal=hard,fadedSignal=fade))
    for case in cases(suite):
        key=case['name']
        for index,(left,right) in enumerate(zip(frames['indexed'][key],frames['fixed'][key])):
            metrics.append(compatible(left,right,np.ones((HEIGHT,WIDTH),bool),'descriptor-parity-'+key+'-'+str(index)))
    return metrics


def self_test():
    expected=oracle(LEFT);mask=polygon_mask(expected['reflectionPolygon'],.7)
    off=np.zeros((HEIGHT,WIDTH,3),np.uint8);on=off.copy();on[mask]=[210,30,10]
    measure_reflection(on,off,expected,'synthetic-correct-location')
    # A changed screenshot at the source or wrong floor position must fail.
    rejected=[]
    at_source=off.copy();at_source[polygon_mask(expected['sourcePolygon'])]=[210,30,10]
    for name,image in [('unchanged',off),('wrong-color',np.where(mask[...,None],np.array([0,0,210],np.uint8),off)),
        ('wrong-position',np.roll(on,55,axis=1)),('physical-source-only',at_source)]:
        try:measure_reflection(image,off,expected,name)
        except AssertionError:rejected.append(name)
        else:raise AssertionError('oracle accepted '+name)
    assert len(rejected)==4
    assert not np.any(mask&polygon_mask(expected['sourcePolygon']))
    assert np.min(np.array(oracle(4.1)['sourcePolygon'])[:,0])>WIDTH,'offscreen source must be completely outside viewport'
    print(json.dumps(dict(status='passed',gpuExecuted=False,oracleNegativeControls=rejected)))


def run(args):
    root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    report=dict(schemaVersion=1,status='preparing',gpuExecuted=False,suite=args.suite,
        budgets=BUDGETS,scriptSha256=sha(Path(__file__)),captures=[],metrics=[],
        limitations=['Native SSR producer and pixel parity require independent evidence.',
            'Live resize and in-process scene hot reload require a host control fixture.',
            'Core uses an emissive opaque source; extended includes point-lit source with zero emissive.'])
    if not args.prepare_only:
        report['playerSha256']=sha(args.executable)
        report['shaderSha256']={p.name:sha(p) for p in (args.executable.parent/'shaders').glob('*.spv')}
    frames={backend:{} for backend in ('indexed','fixed')}
    try:
        self_test()
        for backend in frames:
            for case in cases(args.suite):
                name=backend+'-'+case['name']
                print(json.dumps(dict(capture=name,status='preparing' if args.prepare_only else 'starting')),flush=True)
                images,record=capture(args.executable,root/name,backend,case,args.prepare_only,args.expected_validation)
                report['captures'].append(record)
                report['gpuExecuted'] |= record['gpuExecuted']
                if images is not None:frames[backend][case['name']]=images
        if not args.prepare_only:
            report['metrics']=evaluate(frames,args.suite)
            assert report['playerSha256']==sha(args.executable),'Player changed during capture'
            assert report['shaderSha256']=={p.name:sha(p) for p in (args.executable.parent/'shaders').glob('*.spv')},'shaders changed during capture'
        assert report['scriptSha256']==sha(Path(__file__)),'script changed during capture'
        report['status']='prepared' if args.prepare_only else 'passed'
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:write_json(root/'summary.json',report)
    print(json.dumps(dict(status=report['status'],gpuExecuted=report['gpuExecuted'],captures=len(report['captures']),suite=args.suite)))


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--suite',choices=('core','extended'),default='core')
    parser.add_argument('--prepare-only',action='store_true')
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--expected-validation',choices=('any','enabled','disabled'),default='any')
    args=parser.parse_args()
    if args.self_test:self_test();return
    if not args.output:
        parent=Path(__file__).resolve().parents[1]/'build/runs'
        parent.mkdir(parents=True,exist_ok=True)
        args.output=Path(tempfile.mkdtemp(prefix='screen-reflections-',dir=parent))/'capture'
    if not args.prepare_only and not args.executable:parser.error('--executable is required for GPU execution')
    if args.executable:args.executable=args.executable.resolve()
    run(args)


if __name__=='__main__':main()
