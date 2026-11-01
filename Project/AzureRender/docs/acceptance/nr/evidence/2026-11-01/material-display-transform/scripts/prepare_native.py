import copy,hashlib,json,os,shutil,uuid
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
prior=root/'build/runs/20261101-021345-material-radiance-normalization-1e6231'
source=prior/'project';project=run/'project';assert not project.exists()
immutable={'.glb','.bin','.dds','.ktx2','.png','.jpg','.jpeg','.exr','.azic','.azlut','.azfog','.hdr','.fbx'}
linked=copied=0
for directory,dirs,files in os.walk(source):
    dirs[:]=[d for d in dirs if d!='.azure'];relative=Path(directory).relative_to(source)
    target_dir=project/relative;target_dir.mkdir(parents=True,exist_ok=True)
    for name in files:
        src=Path(directory)/name;target=target_dir/name
        assert target.resolve().is_relative_to(project.resolve())
        if src.suffix.lower() in immutable:os.link(src,target);linked+=1
        else:shutil.copy2(src,target);copied+=1
def save(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
materials=[];identity_map={};folder=project/'assets/linear-display-materials';folder.mkdir()
for path in sorted((project/'assets/normalized-character-materials').glob('*.azurematerial')):
    document=json.loads(path.read_text());metadata=json.loads(Path(str(path)+'.azmeta').read_text())
    profile=document['surface']['materials'][0]['extras']['azureRenderMaterial']
    multiply=profile.get('composite',{}).get('mode')=='multiply';policy='scene' if multiply else 'linear'
    profile['displayTransform']=policy;identity=str(uuid.uuid5(uuid.NAMESPACE_URL,'material-display-linear:'+metadata['id']))
    target=folder/path.name;save(target,document);original_id=metadata['id'];metadata['id']=identity;save(Path(str(target)+'.azmeta'),metadata)
    identity_map[original_id]=identity
    materials.append(dict(path=str(target),displayTransform=policy,radianceScale=profile['radianceScale'],sourceId=original_id,id=identity))
inputs=json.loads((prior/'native-inputs.json').read_text());cases=[]
for previous in inputs['cases']:
    if previous['name']=='near-source-isolated':continue
    case=copy.deepcopy(previous);name=previous['name'].replace('normalized','linear')
    descriptor=json.loads((source/Path(previous['project']).name).read_text())
    level=json.loads((source/'scene'/Path(previous['level']).name).read_text())
    hero=next(n for n in level['nodes'] if n['id']=='hero')
    hero['materialSlots']={key:identity_map.get(value,value) for key,value in hero['materialSlots'].items()}
    descriptor.update(id=name,name=name,startupScene='stage:/'+name+'.azurelevel')
    path=project/(name+'.azureproject');level_path=project/'scene'/(name+'.azurelevel')
    save(path,descriptor);save(level_path,level)
    case.update(name=name,project=str(path),level=str(level_path),displayPolicy='linear')
    cases.append(case)
save(run/'native-inputs.json',dict(status='prepared',project=str(project),sourceProject=str(source),linkedFiles=linked,copiedFiles=copied,materials=materials,cases=cases))
print(json.dumps(dict(cases=len(cases),materials=len(materials),linkedFiles=linked,copiedFiles=copied)))
