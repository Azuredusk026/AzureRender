import copy,hashlib,json,os,shutil,sys,uuid
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;project=run/'material-ab';project.mkdir(exist_ok=True)
sys.path.insert(0,str(root/'tools'))
from retarget_animation import read_glb
source=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project'
current=root/'build/runs/20261031-231334-receiver-directional-light-911ab7/project'
for directory in ['native-character','native-assets','high','profiles','textures','levels']:(project/directory).mkdir(exist_ok=True)
for path in (project/'textures').glob('*'):
    stem=path.name.removesuffix('.azmeta').split('.')[0]
    try:uuid.UUID(stem)
    except ValueError:
        target_dir=run/'material-ab-unused-textures';target_dir.mkdir(exist_ok=True)
        assert path.resolve().is_relative_to(project.resolve()) and not (target_dir/path.name).exists()
        path.rename(target_dir/path.name)
def cp(path,target):
    shutil.copy2(path,target)
    meta=Path(str(path)+'.azmeta')
    if meta.exists():shutil.copy2(meta,Path(str(target)+'.azmeta'))
cp(source/'native-character/outline-converted.glb',project/'native-character/outline-converted.glb')
cp(source/'native-assets/captured-rim-policy.azurematerial',project/'native-assets/captured-rim-policy.azurematerial')
cp(source/'native-character/outline-converted.azurematerial',project/'high/reference.azurematerial')
original,_=read_glb(source/'native-character/outline-converted.glb')
for entry in original.get('images',[]):
    uri=entry.get('uri','')
    if uri and not uri.startswith('data:'):
        src=(source/'native-character'/uri).resolve();target=(project/'native-character'/uri).resolve()
        assert src.is_relative_to((source/'native-character').resolve()) and target.is_relative_to((project/'native-character').resolve())
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,target)
baseline=json.loads((source/'temporal/reference.azurelevel').read_text())
profile_dir=current/'assets/restored-character-materials'
def meta_map(base):
    result={}
    for directory,dirs,files in os.walk(base):
        dirs[:]=[d for d in dirs if d!='.azure']
        for file in files:
            if file.endswith('.azmeta'):
                path=Path(directory)/file;meta=json.loads(path.read_text(encoding='utf-8-sig'));result[meta['id']]=path.with_suffix('')
    return result
metas=meta_map(current);old_metas=meta_map(source)
refdoc=json.loads((source/'native-character/outline-converted.azurematerial').read_text())
for binding in refdoc['textures'].values():
    path=old_metas[binding['asset']];cp(path,project/'textures'/(binding['asset']+path.suffix))
ids={};sources=[]
for material_index in [8,9]:
    src=profile_dir/f'material-{material_index}.azurematerial';doc=json.loads(src.read_text());mat=doc['surface']['materials'][0]
    prior=next(m for m in original['materials'] if m['name']==mat['name'])['extras']['azureRenderMaterial']
    profile=mat['extras']['azureRenderMaterial']
    if 'faceSdf' in prior:
        for field in ['headNode','headBasis']:profile['faceSdf'][field]=copy.deepcopy(prior['faceSdf'][field])
        profile['styleParameters']=profile.get('styleParameters',[.85,.75,.15,.25]);profile['styleParameters'][3]=0
    if 'hair' in prior:profile['hair']['projectedRamp']['frame']=copy.deepcopy(prior['hair']['projectedRamp']['frame'])
    for binding in doc['textures'].values():
        path=metas[binding['asset']];target=project/'textures'/(binding['asset']+path.suffix)
        if not target.exists():cp(path,target)
        else:assert target.read_bytes()==path.read_bytes()
    ident=str(uuid.uuid5(uuid.NAMESPACE_URL,'native-material-ab:'+str(material_index)))
    target=project/'profiles'/f'current-{material_index}.azurematerial';target.write_text(json.dumps(doc,indent=2)+'\n')
    Path(str(target)+'.azmeta').write_text(json.dumps({'schemaVersion':1,'id':ident,'settings':{},'dependencies':list(doc['textures'][k]['asset'] for k in doc['textures'])})+'\n')
    ids[mat['name']]=ident
    sources.append({'file':str(src.relative_to(root)),'sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'staticFrameAdjustment':prior.get('hair',prior.get('faceSdf',{}))})
    if material_index==9:
        one=copy.deepcopy(doc);one['surface']['materials'][0]['extras']['azureRenderMaterial']['hair']['projectedRamp']['lighting']['characterVisibility']='one'
        target=project/'profiles/current-9-one.azurematerial';target.write_text(json.dumps(one,indent=2)+'\n');one_id=str(uuid.uuid5(uuid.NAMESPACE_URL,'native-material-ab:9-one'))
        Path(str(target)+'.azmeta').write_text(json.dumps({'schemaVersion':1,'id':one_id,'settings':{},'dependencies':list(doc['textures'][k]['asset'] for k in doc['textures'])})+'\n')
cases=[]
for name in ['reference','current-head','current-head-visibility-one']:
    level=copy.deepcopy(baseline);hero=next(n for n in level['nodes'] if n['id']=='hero')
    if name!='reference':hero['materialSlots'].update(ids)
    if name.endswith('visibility-one'):hero['materialSlots']['name:M_actor_laevat_hair_01' if 'name:M_actor_laevat_hair_01' in ids else 'M_actor_laevat_hair_01']=one_id
    # Public material slot keys are explicit names.
    for key in list(hero['materialSlots']):
        if not key.startswith('name:'):hero['materialSlots']['name:'+key]=hero['materialSlots'].pop(key)
    level['renderSettings']['antiAliasing']=3
    path=project/'levels'/f'{name}.azurelevel';path.write_text(json.dumps(level,indent=2)+'\n')
    descriptor={'schemaVersion':1,'id':name,'name':name,'mounts':[{'name':'assets','path':'native-assets'},{'name':'character','path':'native-character'},{'name':'reference','path':'high'},{'name':'profiles','path':'profiles'},{'name':'textures','path':'textures'},{'name':'levels','path':'levels'}],'startupScene':f'levels:/{name}.azurelevel'}
    project_path=project/f'{name}.azureproject';project_path.write_text(json.dumps(descriptor,indent=2)+'\n')
    cases.append({'name':name,'project':str(project_path),'captureFrames':32})
(run/'material-ab-cases.json').write_text(json.dumps(cases,indent=2)+'\n');(run/'material-ab-inputs.json').write_text(json.dumps({'sources':sources,'scope':'Static source geometry, native camera and ten lights, temporal AA32. Only face/hair full material documents change; static material frame is explicitly retained.'},indent=2)+'\n')
print(json.dumps(cases))
