"""Check independent coverage dimensions, channels and pass parity in Player."""
import argparse,base64,copy,hashlib,json,os,re,struct,subprocess,sys,uuid
from pathlib import Path
import numpy as np
from PIL import Image
from create_visibility_fixture import component,surface,pad_material_profile_index
from test_object_shadow_gpu import hair_receiver
from native_render.light_lab import generate
from test_outline_projection_gpu import sphere

def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf8')
def dds(values):
    # Authored levels deliberately avoid coverage-preserving mip generation.
    height,width=values.shape[:2];header=bytearray(148)
    def word(at,value):struct.pack_into('<I',header,at,value)
    for at,value in [(0,0x20534444),(4,124),(8,0x2100f),(12,height),(16,width),(20,width*4),(28,5),
                     (76,32),(80,4),(84,0x30315844),(108,0x401008),(128,28),(132,3),(140,1)]:word(at,value)
    levels=[]
    for level in range(5):
        extent=(max(width>>level,1),max(height>>level,1))
        # Author data channels independently. RGBA image resizing would
        # premultiply red coverage by the unrelated alpha test channel.
        image=np.stack([np.asarray(Image.fromarray(values[...,channel]).resize(extent,Image.Resampling.BOX)) for channel in range(4)],axis=2)
        levels.append(image.tobytes())
    return bytes(header)+b''.join(levels)

def prepare(folder,kind,policy,channel,aa,shadow=False):
    folder.mkdir(parents=True)
    yy,xx=np.mgrid[:16,:16];mask=np.where(((xx*3+yy*5)%13)<6,35,220).astype(np.uint8)
    rgba=np.empty((16,16,4),dtype=np.uint8);rgba[...,:3]=255;rgba[...,0]=mask;rgba[...,3]=255-mask
    canonical=rgba.copy();canonical[...,:3]=255;canonical[...,3]=mask if channel=='red' else 255-mask
    (folder/'coverage.dds').write_bytes(dds(canonical if policy=='reference' else rgba))
    write(folder/'coverage.dds.azmeta',dict(schemaVersion=1,id=str(uuid.uuid4()),dependencies=[],settings=dict(texture=dict(usage='mask',colorSpace='linear',filter='linear',mipFilter='nearest',anisotropy=1,mipBias=-1))))
    if shadow:
        generate(folder/'lab','directional-near');work=folder/'lab';path=work/'plane.gltf';model=json.loads(path.read_text())
        for name in ['coverage.dds','coverage.dds.azmeta']:(work/name).write_bytes((folder/name).read_bytes())
        levelPath=work/'level.azurelevel';level=json.loads(levelPath.read_text());level['lights'][0]['shape']['direction']=[.6,0,-.8];level['renderSettings'].update(shadowRadius=1,shadowDistance=20)
    else:
        work=folder;path=work/'surface.gltf';surface(path,double_sided=True);model=json.loads(path.read_text())
        raw=base64.b64decode(model['buffers'][0]['uri'].split(',')[1])+struct.pack('<8f',0,1,1,1,1,0,0,0)
        model['buffers'][0]=dict(byteLength=len(raw),uri='data:application/octet-stream;base64,'+base64.b64encode(raw).decode())
        model['bufferViews'].append(dict(buffer=0,byteOffset=len(raw)-32,byteLength=32));model['accessors'].append(dict(bufferView=3,componentType=5126,count=4,type='VEC2'));model['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0']=3
        if kind=='hair-shell':
            model,payload,_=sphere();model['buffers']=[dict(byteLength=len(payload),uri='data:application/octet-stream;base64,'+base64.b64encode(payload).decode())]
            model['materials']=[dict(pbrMetallicRoughness=dict(baseColorFactor=[1,1,1,1],metallicFactor=0,roughnessFactor=1))]
        camera=component('azure.camera',dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=20,orthographic=True,orthographicHeight=3 if kind=='hair-shell' else 1));camera['version']=2
        nodes=[dict(id='surface',resourceId='surface',components={'azure.transform':component('azure.transform',dict(translation=[0,0,0]))}),dict(id='camera',components={'azure.transform':component('azure.transform',dict(translation=[0,0,3])),'azure.camera':camera})]
        level=dict(schemaVersion=1,id='independent-coverage',sceneType='character',resources=[dict(id='surface',asset='assets:/surface.gltf')],nodes=nodes,lights=[],renderSettings=dict(schemaVersion=20,sceneLighting=True,skyIntensity=0,cameraNear=.1,cameraFar=20,shadowDistance=20,platform=False,background=False,silhouetteOutline=False,innerOutline=False,stylized=False,bloomEnabled=False,antiAliasing=aa,exposure=0,toneMapping=False,contrast=1,saturation=1))
        levelPath=work/'scene.azurelevel'
    material=model['materials'][0];material['pbrMetallicRoughness'].pop('metallicRoughnessTexture',None)
    material['pbrMetallicRoughness'].update(baseColorFactor=[1,1,1,1],metallicFactor=0,roughnessFactor=.6)
    material.update(alphaMode='MASK',alphaCutoff=.5,doubleSided=True)
    material.setdefault('extras',{})['azureRenderMaterial']=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface'],surface=dict(schemaVersion=1,lighting='unlit'))
    if kind in ['hair','hair-shell']:
        hair_receiver(material,model,work,'one',True)
        material['extras']['azureRenderMaterial']['hair']['projectedRamp']['lighting'].update(ambientColor=[1,1,1],mainColor=[0,0,0])
        if kind=='hair-shell':
            profile=material['extras']['azureRenderMaterial'];profile['features'].append('silhouette-outline')
            profile['outlineSurface']=dict(schemaVersion=1,mode='view-scaled',width=3,minimumPixels=1,depthPrepass=True,color=[1,0,0])
    else:model['images']=[];model['textures']=[]
    coverageIndex=len(model['images']);model['images'].append(dict(uri='coverage.dds'));model['textures'].append(dict(source=coverageIndex))
    coverageTexture=len(model['textures'])-1
    if policy=='reference':material['pbrMetallicRoughness']['baseColorTexture']=dict(index=coverageTexture)
    else:
        # Base alpha is zero: the independent cutout must own visibility.
        Image.new('RGBA',(64,64),(255,255,255,0)).save(work/'base.png');index=len(model['images']);model['images'].append(dict(uri='base.png'));model['textures'].append(dict(source=index))
        material['pbrMetallicRoughness']['baseColorTexture']=dict(index=len(model['textures'])-1)
        material['extras']['azureRenderMaterial']['coverageTexture']=dict(schemaVersion=1,texture=coverageTexture,channel=channel)
    if shadow:
        level['resources'].append(dict(id='caster',asset='assets:/caster.gltf'));level['nodes'].append(dict(id='caster',resourceId='caster',components={'azure.transform':component('azure.transform',dict(translation=[0,0,1],scale=[.125,.125,1]))}));path=work/'caster.gltf'
        if shadow=='object':
            receiver=next(node for node in level['nodes'] if node.get('resourceId')=='plane')
            receiver.setdefault('components',{})['azure.object-shadow']=component('azure.object-shadow',dict(enabled=True,center=[0,0,.5],halfExtents=[6,6,3],normalBias=.1,lightBias=.05,filterRadiusX=4,filterRadiusY=2,atlasPage=6))
            # Object pages own the component's complete asset instance.
            # Put receiver and masked caster in that same source asset.
            receiverMaterial=copy.deepcopy(json.loads((work/'plane.gltf').read_text())['materials'][0])
            model['materials'].append(receiverMaterial);receiverMesh=copy.deepcopy(model['meshes'][0])
            for primitive in receiverMesh['primitives']:primitive['material']=1
            model['meshes'].append(receiverMesh);model['nodes'][0]['mesh']=1
            model['nodes'].append(dict(mesh=0,translation=[0,0,1],scale=[.125,.125,1]));model['scenes'][0]['nodes'].append(len(model['nodes'])-1)
            level['resources']=[resource for resource in level['resources'] if resource['id']!='caster'];level['nodes']=[node for node in level['nodes'] if node['id']!='caster'];path=work/'plane.gltf'
    pad_material_profile_index(model);write(path,model);write(levelPath,level)
    project=work/('lab.azureproject' if shadow else 'project.azureproject')
    if not shadow:write(project,dict(schemaVersion=1,id='independent-coverage',name='Independent coverage',mounts=[dict(name='assets',path='.')],startupScene='assets:/scene.azurelevel'))
    return project

def run(player,output,validation='any',selected=None):
    output.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    report=dict(status='running',programSha256=sha(player),toolSha256=sha(Path(__file__)),cases=[],groups=[]);images={}
    cases=[(kind,channel,aa,False) for kind in ['generic','hair'] for channel in ['red','alpha'] for aa in [0,3,4]]
    cases += [('generic','red',0,'cascade'),('generic','red',0,'object')]
    cases += [('hair-shell','red',aa,False) for aa in [0,3,4]]
    caseName=lambda case:'object-shadow' if case[3]=='object' else 'shadow' if case[3] else case[0]+'-'+case[1]+'-aa'+str(case[2])
    if selected:
        assert set(selected)<={caseName(case) for case in cases};cases=[case for case in cases if caseName(case) in selected]
    env=os.environ.copy();env.setdefault('DOTNET_ROOT','D:/Tools/dotnet9');env.pop('AZURERENDER_RESOURCE_ROOT',None)
    startup=None
    if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    try:
        for kind,channel,aa,shadow in cases:
            group=caseName((kind,channel,aa,shadow))
            for backend in ['indexed','fixed']:
                for policy in ['reference','independent']:
                    label=group+'-'+backend+'-'+policy;folder=output/label;print('START',label,flush=True)
                    project=prepare(folder,kind,policy,channel,aa,shadow);frames=8 if aa==3 else 1
                    command=[str(player),'--project',str(project),'--width','256' if shadow else '128','--height','256' if shadow else '128','--capture-frames',str(frames),'--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--capture-dir',str(folder/'frames'),'--runtime-report',str(folder/'runtime.json')]
                    if shadow:command+=['--qa-isolation','object-shadow-visibility' if shadow=='object' else 'shadow-visibility']
                    if aa:command+=['--anti-aliasing','temporal' if aa==3 else 'multisample']
                    if backend=='fixed':command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                    inputs={p.relative_to(folder).as_posix():sha(p) for p in folder.rglob('*') if p.is_file()}
                    with (folder/'player.log').open('wb') as log:result=subprocess.run(command,cwd=project.parent,env=env,startupinfo=startup,stdout=log,stderr=log,timeout=180)
                    assert result.returncode==0,label
                    log=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log),label
                    assert 'Allocator after unload: buffers=0 images=0' in log,label
                    if validation!='any':assert ('Validation layer: enabled' in log)==(validation=='enabled'),label
                    manifest=json.loads((folder/'frames/capture_manifest.json').read_text());sources=manifest['materialCoverageSources'];assert len(sources)==(policy=='independent'),label
                    if shadow=='object':assert manifest['objectShadowPages']==1,label
                    if sources:assert sources[0]['width']==16 and sources[0]['height']==16 and sources[0]['authoredMipLevels']==5 and sources[0]['channel']==channel,label
                    runtime=json.loads((folder/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors'],label
                    image=np.asarray(Image.open(folder/'frames'/f'frame_{frames-1:06d}.png').convert('RGB'));images[label]=image
                    for file,digest in inputs.items():assert sha(folder/file)==digest,file
                    report['cases'].append(dict(name=label,command=command,inputs=inputs,frames=frames,imageSha256=sha(folder/'frames'/f'frame_{frames-1:06d}.png'),status='captured'))
                a=images[group+'-'+backend+'-reference'];b=images[group+'-'+backend+'-independent']
                assert np.array_equal(a,b),(group,backend,int(abs(a.astype(int)-b.astype(int)).max()),int(np.count_nonzero(np.any(a!=b,axis=2))))
                assert a.max()-a.min()>20,(group,'positive coverage control')
            assert np.array_equal(images[group+'-indexed-independent'],images[group+'-fixed-independent']),group
            report['groups'].append(dict(name=group,status='passed',comparison='pixel-exact-reference-and-descriptor-paths'))
            print('PASS',group,flush=True)
        assert sha(player)==report['programSha256'];report['status']='passed'
    finally:
        if report['status']=='running':report['status']='failed'
        write(output/'summary.json',report)

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');parser.add_argument('--case',action='append');args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];task=None
    if args.output is None:task=Path(subprocess.check_output([sys.executable,str(root/'tools/artifact_cleanup.py'),'--new-run','material-coverage'],cwd=root,text=True).strip());args.output=task/'captures'
    try:run(args.executable.resolve(),args.output.resolve(),args.expected_validation,args.case)
    finally:
        if task:subprocess.run([sys.executable,str(root/'tools/artifact_cleanup.py'),'--finish',str(task)],cwd=root,check=True)
