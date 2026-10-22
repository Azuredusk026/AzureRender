"""Exercise explicit instance silhouettes with a curved strand above a base.

The fixture is procedural CC0 geometry and consumes public glTF, level and
Player APIs. Preparation never starts Player. Capture requires --run.
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
import tempfile

import numpy as np
from PIL import Image


WIDTH, HEIGHT = 384, 288
FRONT = {'translation': [-.26, 0, .4], 'scale': [.72, .72, .72]}
BACK = {'translation': [.15, 0, -.25], 'scale': [1.35, 1.35, 1.35]}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2), encoding='utf-8')


def fixture(explicit=True):
    positions, normals, indices = [], [], []

    def triangle(a, b, c):
        pa, pb, pc = (np.asarray(positions[i]) for i in (a, b, c))
        geometric = np.cross(pb-pa, pc-pa)
        assert np.linalg.norm(geometric) > 1e-9
        expected = np.asarray(normals[a])+normals[b]+normals[c]
        if np.dot(geometric, expected) < 0:
            b, c = c, b
        indices.extend((a, b, c))

    def quad(points, normal):
        first = len(positions)
        positions.extend(points)
        normals.extend([normal]*4)
        triangle(first, first+1, first+2)
        triangle(first, first+2, first+3)

    # A closed thin base gives an independently visible outer silhouette.
    x, y, near, far = .65, .65, 0, -.08
    quad([(-x,-y,near),(x,-y,near),(x,y,near),(-x,y,near)], (0,0,1))
    quad([(-x,-y,far),(x,-y,far),(x,y,far),(-x,y,far)], (0,0,-1))
    quad([(x,-y,far),(x,y,far),(x,y,near),(x,-y,near)], (1,0,0))
    quad([(-x,-y,far),(-x,y,far),(-x,y,near),(-x,-y,near)], (-1,0,0))
    quad([(-x,y,far),(x,y,far),(x,y,near),(-x,y,near)], (0,1,0))
    quad([(-x,-y,far),(x,-y,far),(x,-y,near),(-x,-y,near)], (0,-1,0))

    # A closed bent strand floats in front of the base. Its outward shell
    # projects inside that same instance's base, yielding the regression.
    first = len(positions)
    rings, sides = 17, 12
    for ring in range(rings):
        v = ring/(rings-1)
        cy = (v-.5)*1.02
        cx = .16*math.sin(cy*math.pi)
        tangent_x = .16*math.pi*math.cos(cy*math.pi)
        for side in range(sides):
            angle = 2*math.pi*side/sides
            dx, dz = .085*math.cos(angle), .028*math.sin(angle)
            positions.append((cx+dx, cy, .2+dz))
            normal = np.array([math.cos(angle)/.085,
                -tangent_x*math.cos(angle)/.085, math.sin(angle)/.028])
            normals.append(tuple(normal/np.linalg.norm(normal)))
    for ring in range(rings-1):
        for side in range(sides):
            a = first+ring*sides+side
            b = first+ring*sides+(side+1)%sides
            triangle(a,b,b+sides)
            triangle(a,b+sides,a+sides)
    for ring, direction in ((0,-1),(rings-1,1)):
        cy = (ring/(rings-1)-.5)*1.02
        cx = .16*math.sin(cy*math.pi)
        tangent_x = .16*math.pi*math.cos(cy*math.pi)
        normal = np.array([-tangent_x*direction, direction, 0.])
        normal /= np.linalg.norm(normal)
        center = len(positions)
        positions.append((cx,cy,.2)); normals.append(tuple(normal))
        for side in range(sides):
            # Caps use separate normals so triangle checks are independent.
            edge = len(positions)
            for j in (side,(side+1)%sides):
                positions.append(positions[first+ring*sides+j])
                normals.append(tuple(normal))
            triangle(center,edge,edge+1)

    pos = np.asarray(positions, dtype='<f4')
    norm = np.asarray(normals, dtype='<f4')
    index = np.asarray(indices, dtype='<u2')
    time_values = struct.pack('<6f',0,1,2,1,.5,0)
    binary = pos.tobytes()+norm.tobytes()+index.tobytes()
    binary += bytes((-len(binary))%4)
    animation_offset = len(binary)
    binary += time_values
    profile = {'schemaVersion': 2, 'class': 'skin',
        'features': ['silhouette-outline'] if explicit else []}
    model = {'asset': {'version': '2.0', 'generator': 'Azure silhouette fixture',
            'extras': {'license': 'CC0-1.0'}},
        'scene': 0, 'scenes': [{'nodes':[0]}],
        'extensionsUsed': ['AZURE_node_opacity'],
        'nodes': [{'mesh':0, 'extensions': {'AZURE_node_opacity': {
            'schemaVersion':1, 'opacity':1}}}],
        'materials': [{'pbrMetallicRoughness': {'baseColorFactor':[.9,.12,.04,1],
            'metallicFactor':0,'roughnessFactor':1},
            'extras': {'azureRenderMaterial':profile}}],
        'meshes':[{'primitives':[{'attributes':{'POSITION':0,'NORMAL':1},
            'indices':2,'material':0,'mode':4}]}],
        'buffers':[{'byteLength':len(binary),'uri':
            'data:application/octet-stream;base64,'+base64.b64encode(binary).decode()}],
        'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':pos.nbytes},
            {'buffer':0,'byteOffset':pos.nbytes,'byteLength':norm.nbytes},
            {'buffer':0,'byteOffset':pos.nbytes+norm.nbytes,'byteLength':index.nbytes},
            {'buffer':0,'byteOffset':animation_offset,'byteLength':12},
            {'buffer':0,'byteOffset':animation_offset+12,'byteLength':12}],
        'accessors':[{'bufferView':0,'componentType':5126,'count':len(pos),'type':'VEC3',
            'min':pos.min(axis=0).tolist(),'max':pos.max(axis=0).tolist()},
            {'bufferView':1,'componentType':5126,'count':len(norm),'type':'VEC3'},
            {'bufferView':2,'componentType':5123,'count':len(index),'type':'SCALAR'},
            {'bufferView':3,'componentType':5126,'count':3,'type':'SCALAR','min':[0],'max':[2]},
            {'bufferView':4,'componentType':5126,'count':3,'type':'SCALAR'}],
        'animations':[{'name':'fade','channels':[],
            'samplers':[{'input':3,'output':4,'interpolation':'STEP'}],
            'extensions':{'AZURE_node_opacity':{'schemaVersion':1,
                'channels':[{'node':0,'sampler':0}]}}}]}
    return model


def component(kind, data, version=1):
    return {'type':kind,'version':version,'data':data}


def backface_card(model):
    """Add an unoutlined single-sided quad facing away from the camera.

    Primary opaque rendering must cull this primitive. Its projected area
    covers the base's valid exterior rim, exposing incorrect mask culling.
    """
    model=copy.deepcopy(model)
    positions=np.asarray([[-.95,-.95,.32],[.95,-.95,.32],[.95,.95,.32],[-.95,.95,.32]],dtype='<f4')
    normals=np.asarray([[0,0,-1]]*4,dtype='<f4')
    indices=np.asarray([0,2,1,0,3,2],dtype='<u2')
    binary=positions.tobytes()+normals.tobytes()+indices.tobytes()
    buffer=len(model['buffers']);view=len(model['bufferViews']);accessor=len(model['accessors'])
    model['buffers'].append({'byteLength':len(binary),'uri':
        'data:application/octet-stream;base64,'+base64.b64encode(binary).decode()})
    model['bufferViews'] += [{'buffer':buffer,'byteOffset':0,'byteLength':48},
        {'buffer':buffer,'byteOffset':48,'byteLength':48},
        {'buffer':buffer,'byteOffset':96,'byteLength':12}]
    model['accessors'] += [{'bufferView':view,'componentType':5126,'count':4,'type':'VEC3',
        'min':positions.min(axis=0).tolist(),'max':positions.max(axis=0).tolist()},
        {'bufferView':view+1,'componentType':5126,'count':4,'type':'VEC3'},
        {'bufferView':view+2,'componentType':5123,'count':6,'type':'SCALAR'}]
    material=copy.deepcopy(model['materials'][0]);material['doubleSided']=False
    material['extras']['azureRenderMaterial']['features']=[]
    model['meshes'][0]['primitives'].append({'attributes':{'POSITION':accessor,'NORMAL':accessor+1},
        'indices':accessor+2,'material':len(model['materials']),'mode':4})
    model['materials'].append(material)
    return model


def cases():
    def subject(label='subject', age=0, transform=None):
        return {'id':label,'resourceId':'subject','components':{
            'azure.transform':component('azure.transform',transform or {}),
            'azure.animator':component('azure.animator',{'asset':'assets:/motion.json',
                'state':'fade','startTime':age},2)}}
    result = []
    for name, age, explicit in [('blank',2,False),('single-plain',0,False),
            ('single-full',0,True),('single-half-plain',1,False),('single-half',1,True)]:
        result.append({'name':name,'explicit':explicit,'nodes':[subject(age=age)]})
    for name, age, explicit in [('front-plain',0,False),('front-full',0,True),
            ('front-half-plain',1,False),('front-half',1,True)]:
        result.append({'name':name,'explicit':explicit,'nodes':[subject('front',age,FRONT)]})
    result.append({'name':'back-plain','explicit':False,'nodes':[subject('back',0,BACK)]})
    for name, age, fixed in [('pair-full',0,False),('pair-half',1,False),('pair-half-fixed',1,True)]:
        result.append({'name':name,'explicit':True,'fixed':fixed,
            'nodes':[subject('front',age,FRONT),subject('back',0,BACK)]})
    result.append({'name':'legacy-full','explicit':False,'legacy':True,'nodes':[subject()]})
    for name,age,explicit in [('backface-plain',0,False),('backface-half',1,True),
            ('backface-full',0,True)]:
        result.append({'name':name,'explicit':explicit,'backface':True,'nodes':[subject(age=age)]})
    for name,card in [('mirrored-half',False),('mirrored-backface-half',True)]:
        result.append({'name':name,'explicit':True,'backface':card,
            'nodes':[subject(age=1,transform={'scale':[-1,1,1]})]})
    return result


def prepare(root, player):
    root.mkdir(parents=True,exist_ok=False)
    records = []
    for case in cases():
        path = root/case['name']; assets = path/'assets'; assets.mkdir(parents=True)
        model=fixture(case['explicit'])
        if case.get('backface'):model=backface_card(model)
        write_json(assets/'subject.gltf',model)
        write_json(assets/'motion.json',{'schemaVersion':1,'initial':'fade',
            'states':[{'name':'fade','clip':0,'loop':False}],'transitions':[]})
        nodes = copy.deepcopy(case['nodes'])
        nodes.append({'id':'camera','components':{
            'azure.transform':component('azure.transform',{'translation':[0,0,3]}),
            'azure.camera':component('azure.camera',{'enabled':True,'priority':1,'fieldOfView':55})}})
        write_json(assets/'scene.azurelevel',{'schemaVersion':1,'id':case['name'],
            'sceneType':'character','resources':[{'id':'subject','asset':'assets:/subject.gltf'}],
            'nodes':nodes,'renderSettings':{'platform':False,
                'silhouetteOutline':case.get('legacy',False),'innerOutline':False,
                'outlinePixels':1.5,'stylized':False,'sceneLighting':False,
                'bloomEnabled':False,'antiAliasing':0,'skyMode':2,'skyColor':[.06,.12,.22],
                'exposure':0,'toneMapping':False,'contrast':1,'saturation':1}})
        project = path/'test.azureproject'
        write_json(project,{'schemaVersion':1,'id':'silhouette-coverage','name':'Silhouette coverage',
            'mounts':[{'name':'assets','path':'assets'}],'startupScene':'assets:/scene.azurelevel'})
        command = [str(player),'--project',str(project),'--width',str(WIDTH),'--height',str(HEIGHT),
            '--capture-dir',str(path/'frames'),'--capture-frames','1','--fixed-frame-step',
            '--qa-camera','scene','--qa-light','scene','--qa-isolation','albedo',
            '--runtime-report',str(path/'runtime.json')]
        if case.get('fixed'):
            command += ['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
        write_json(path/'command.json',command)
        records.append({'name':case['name'],'assetSha256':digest(assets/'subject.gltf')})
    write_json(root/'preparation.json',{'status':'prepared-not-captured','width':WIDTH,'height':HEIGHT,
        'geometry':'closed bent strand and closed opaque base in one glTF instance',
        'cases':records,'scriptSha256':digest(Path(__file__))})


def capture(root, player):
    assert player.is_file(),player
    records=[]
    for case in cases():
        path = root/case['name']
        assert not (path/'frames').exists(),f'Refusing to overwrite capture: {path}'
        command=json.loads((path/'command.json').read_text())
        assert Path(command[0]).resolve()==player.resolve(),'Prepared Player differs'
        run=subprocess.run(command,capture_output=True,encoding='utf-8',errors='replace',timeout=120)
        log=run.stdout+run.stderr;(path/'player.log').write_text(log,encoding='utf-8')
        assert run.returncode==0,(case['name'],run.returncode,log[-3000:])
        assert not re.search(r'VUID-|Validation Error|\[validation warning\]|\[error\]',log),case['name']
        assert 'Allocator after unload: buffers=0 images=0' in log,case['name']
        runtime=json.loads((path/'runtime.json').read_text())
        assert not runtime['scriptErrors'] and not runtime['presentationErrors'],case['name']
        records.append({'name':case['name'],'validationEnabled':'Validation layer: enabled' in log,
            'maskAllocations':re.findall(r'Outline mask allocated:[^\r\n]+',log)})
        write_json(root/'capture-results.json',{'playerSha256':digest(player),'captures':records})


def erode(mask, radius):
    result=mask.copy()
    for _ in range(radius):
        result[0]=False;result[-1]=False;result[:,0]=False;result[:,-1]=False
        result[1:-1,1:-1] &= (mask[:-2,1:-1]&mask[2:,1:-1]&mask[1:-1,:-2]&mask[1:-1,2:])
        mask=result.copy()
    return result


def verify(root):
    frames={case['name']:np.asarray(Image.open(root/case['name']/'frames/frame_000000.png')
        .convert('RGB'),dtype=np.int16) for case in cases()}
    background=frames['blank']; metrics={}; failures=[]
    def check(condition, name):
        if not condition:failures.append(name)
    def surface(image):
        return (image[:,:,0]>image[:,:,2]+30)&(image[:,:,0]>image[:,:,1]+30)
    def dark(image):
        return image.max(axis=2)<90
    for plain_name, outline_name in [('single-plain','single-full'),('single-half-plain','single-half')]:
        interior=erode(surface(frames['single-plain']),6)
        difference=np.max(np.abs(frames[plain_name]-frames[outline_name]),axis=2)
        leaked=int(np.count_nonzero(interior&(difference>3)))
        metrics[outline_name+'InteriorChangedPixels']=leaked
        check(int(interior.sum())>5000,'interior positive fixture')
        check(leaked<=2,outline_name+' must preserve opaque coverage interior')
    outside=~surface(frames['single-plain'])
    rim=dark(frames['single-full'])&outside&(np.max(np.abs(frames['single-full']-background),axis=2)>12)
    metrics['outerRimPixels']=int(rim.sum())
    check(int(rim.sum())>120,'full silhouette must retain an exterior rim')
    half_rim=(dark(frames['single-half'])&outside
        &(np.max(np.abs(frames['single-half']-frames['single-half-plain']),axis=2)>12))
    metrics['halfOuterRimPixels']=int(half_rim.sum())
    check(30<int(half_rim.sum())<int(rim.sum()),'fading must preserve a partial exterior rim')
    # Own-instance rim is determined without the rear instance. The rear's
    # unoutlined material independently identifies overlap expectations.
    front_rim=(dark(frames['front-full'])&~surface(frames['front-plain'])
        &(np.max(np.abs(frames['front-full']-frames['front-plain']),axis=2)>12))
    overlap=front_rim&erode(surface(frames['back-plain']),3)
    metrics['overlapExpectedRimPixels']=int(overlap.sum())
    check(int(overlap.sum())>80,'overlap positive fixture')
    missing=int(np.count_nonzero(overlap&~dark(frames['pair-full'])))
    metrics['overlapFullMissingRimPixels']=missing
    check(missing<=2,'rear coverage must retain the foreground full rim')
    half_expected=(overlap&dark(frames['front-half'])
        &(np.max(np.abs(frames['front-half']-frames['front-half-plain']),axis=2)>12))
    metrics['overlapHalfExpectedPixels']=int(half_expected.sum())
    check(int(half_expected.sum())>20,'half overlap positive fixture')
    missing=int(np.count_nonzero(half_expected&~dark(frames['pair-half'])))
    metrics['overlapHalfMissingRimPixels']=missing
    check(missing<=2,'rear coverage must retain the foreground fading rim')
    maximum=int(np.abs(frames['pair-half']-frames['pair-half-fixed']).max())
    metrics['fallbackMaximumDifference']=maximum
    check(maximum<=1,'descriptor and skinning fallbacks must match')
    maximum=int(np.abs(frames['backface-plain']-frames['single-plain']).max())
    metrics['culledCardPrimaryMaximumDifference']=maximum
    check(maximum<=1,'single-sided backface card must remain absent from primary coverage')
    for expected,actual in [('single-half','backface-half'),('single-full','backface-full'),
            ('mirrored-half','mirrored-backface-half')]:
        maximum=int(np.abs(frames[expected]-frames[actual]).max())
        metrics[actual+'MaximumDifference']=maximum
        check(maximum<=1,actual+' must retain the same visible silhouette without the culled card')
    legacy_log=(root/'legacy-full/player.log').read_text(encoding='utf-8')
    check('Outline mask allocated:' not in legacy_log,'legacy full opacity must allocate no mask')
    summary={'passed':not failures,'metrics':metrics,'failures':failures}
    write_json(root/'summary.json',summary)
    print(json.dumps(summary,indent=2),flush=True)
    assert not failures,failures


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--run',action='store_true')
    mode.add_argument('--verify-only',action='store_true')
    args=parser.parse_args();player=args.executable.resolve()
    if args.output is None and not args.run:
        parser.error('--output is required for preparation or verification')
    def execute(root):
        if args.verify_only:verify(root)
        elif args.run:
            if not root.exists():prepare(root,player)
            capture(root,player);verify(root)
        else:prepare(root,player)
    if args.output is not None:execute(args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='azure_silhouette_coverage_') as directory:
            execute(Path(directory)/'evidence')


if __name__=='__main__':main()
