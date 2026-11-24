import copy,hashlib,json,os,shutil,uuid
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;source=root/'build/runs/20261031-231334-receiver-directional-light-911ab7/project';project=run/'project'
project.mkdir(exist_ok=True)
linked=copied=linked_bytes=0
immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azlut','.azfog','.hdr','.fbx'}
for directory,dirs,files in os.walk(source):
    dirs[:]=[d for d in dirs if d!='.azure']
    relative=Path(directory).relative_to(source);target_dir=project/relative;target_dir.mkdir(parents=True,exist_ok=True)
    for name in files:
        src=Path(directory)/name;target=target_dir/name
        assert src.resolve().is_relative_to(source.resolve()) and target.resolve().is_relative_to(project.resolve())
        if target.exists():
            assert hashlib.sha256(src.read_bytes()).digest()==hashlib.sha256(target.read_bytes()).digest(),str(target)
            if src.suffix.lower() in immutable:linked+=1;linked_bytes+=src.stat().st_size
            else:copied+=1
            continue
        if src.suffix.lower() in immutable:
            try:os.link(src,target);linked+=1;linked_bytes+=src.stat().st_size
            except OSError:shutil.copy2(src,target);copied+=1
        else:shutil.copy2(src,target);copied+=1
base_project=json.loads((source/'near-reference-source-display.azureproject').read_text())
reference_near=json.loads((source/'scene/near-reference-source-display.azurelevel').read_text())
world_full=json.loads((source/'scene/full-control.azurelevel').read_text())
original=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/temporal/reference.azurelevel'
portrait=json.loads(original.read_text());npr=copy.deepcopy(portrait['renderSettings']['nprLighting']);npr['specularFloor']=portrait['renderSettings']['nprLighting'].get('specularFloor',0)
source_ev=portrait['renderSettings']['exposure'];target_ev=world_full['renderSettings']['exposure'];scale=2**(source_ev-target_ev)
assert source_ev==-2 and target_ev==0 and scale==.25
material_dir=project/'assets/normalized-character-materials';material_dir.mkdir(exist_ok=True);identity_map={};materials=[]
for path in sorted((project/'assets/restored-character-materials').glob('*.azurematerial')):
    document=json.loads(path.read_text());metadata=json.loads(Path(str(path)+'.azmeta').read_text());material=document['surface']['materials'][0];profile=material.setdefault('extras',{}).setdefault('azureRenderMaterial',{'schemaVersion':2,'class':'generic','features':[]})
    multiplicative=profile.get('composite',{}).get('mode')=='multiply';gain=1 if multiplicative else scale
    # Native character inputs currently contain no layered unlit exposure policy.
    assert 'unlit-texture-layers' not in profile.get('features',[]),material['name']
    profile['radianceScale']=gain
    identity=str(uuid.uuid5(uuid.NAMESPACE_URL,'material-radiance:'+metadata['id']+':'+str(gain)))
    target=material_dir/path.name;target.write_text(json.dumps(document,indent=2)+'\n');new_metadata=copy.deepcopy(metadata);new_metadata['id']=identity
    Path(str(target)+'.azmeta').write_text(json.dumps(new_metadata,indent=2)+'\n')
    identity_map[metadata['id']]=identity
    materials.append({'name':material['name'],'sourceFile':path.relative_to(project).as_posix(),'sourceSha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'normalizedFile':target.relative_to(project).as_posix(),'normalizedSha256':hashlib.sha256(target.read_bytes()).hexdigest(),
        'sourceId':metadata['id'],'normalizedId':identity,'radianceScale':gain,'unitlessMultiply':multiplicative})
cases=[]
for name,normalized,isolated,full,hidden,aa,frames in [
    ('near-source-isolated',False,True,False,False,0,1),
    ('near-normalized-isolated',True,True,False,False,0,1),
    ('near-world-normalized',True,False,False,False,0,1),
    ('near-world-temporal',True,False,False,False,3,32),
    ('full-world-normalized',True,False,True,False,1,12),
    ('full-world-hidden',True,False,True,True,1,12)]:
    level=copy.deepcopy(reference_near);level['renderSettings']=copy.deepcopy(world_full['renderSettings']);level['renderSettings']['nprLighting']=copy.deepcopy(npr);level['renderSettings']['antiAliasing']=aa
    hero=next(n for n in level['nodes'] if n['id']=='hero')
    if normalized:hero['materialSlots']={key:identity_map.get(value,value) for key,value in hero['materialSlots'].items()}
    if hidden:hero['components']['azure.renderable']['data']['visible']=False
    if full:
        camera=next(n for n in level['nodes'] if 'azure.camera' in n.get('components',{}))
        source_camera=next(n for n in world_full['nodes'] if n.get('id')==camera['id'])
        camera['components']['azure.transform']=copy.deepcopy(source_camera['components']['azure.transform'])
        camera['components']['azure.camera']=copy.deepcopy(source_camera['components']['azure.camera'])
    if isolated:
        settings=level['renderSettings'];settings.update(exposure=target_ev if normalized else source_ev,bloomEnabled=False,toneMapping=False,contrast=1,saturation=1)
        settings['gradeLut']={'schemaVersion':1,'asset':'','exposureMultiplier':1}
        settings['screenSpaceReflection']['enabled']=False
        level['viewFog']['enabled']=False
    level_path=project/'scene'/(name+'.azurelevel');level_path.write_text(json.dumps(level,indent=2)+'\n')
    descriptor=copy.deepcopy(base_project);descriptor.update(id=name,name=name,startupScene='stage:/'+level_path.name)
    project_path=project/(name+'.azureproject');project_path.write_text(json.dumps(descriptor,indent=2)+'\n')
    cases.append({'name':name,'project':str(project_path),'level':str(level_path),'captureFrames':frames,'width':2560 if full else 1536,'height':1440 if full else 1536,'normalized':normalized,'isolated':isolated,'antiAliasing':aa})
report={'status':'prepared','scope':'Independent text and metadata copies; immutable graphics payloads share NTFS data through hard links. Original material documents, V8 geometry and animation payloads remain intact. Isolated views disable fog and screen reflections to qualify the linear normalization comparison.',
    'sourceProject':str(source),'project':str(project),'linkedFiles':linked,'linkedBytes':linked_bytes,'copiedFiles':copied,
    'sourceExposureEv':source_ev,'targetExposureEv':target_ev,'radianceScale':scale,'nprSettings':npr,'materials':materials,'cases':cases}
(run/'native-inputs.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(cases),'materials':len(materials),'linkedFiles':linked,'copiedFiles':copied,'linkedBytes':linked_bytes}))
