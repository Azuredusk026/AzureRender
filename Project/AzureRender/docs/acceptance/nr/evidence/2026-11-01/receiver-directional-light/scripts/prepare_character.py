import copy,hashlib,json,math,os,shutil
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
source=root/'build/runs/20261031-183314-full-scene-playable-validation-d5257e/project'
visual=root/'build/runs/20261031-222119-combined-visual-isolation-ad6878/project'
portrait=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/high/reference.azurelevel'
target=run/'project';target.mkdir()
binary={'.glb','.bin','.dds','.png','.jpg','.jpeg','.azlut','.azirr','.azfog','.azic','.azoa','.azenv','.wav','.ogg','.fbx','.azprobe','.ktx2'}
for p in source.rglob('*'):
    if not p.is_file() or '.azure' in p.relative_to(source).parts:continue
    q=target/p.relative_to(source);q.parent.mkdir(parents=True,exist_ok=True)
    if p.suffix.lower() in binary:os.link(p,q)
    else:shutil.copy2(p,q)
ref=json.loads(portrait.read_text());near=json.loads((visual/'scene/control.azurelevel').read_text())
full=json.loads((visual/'scene/full-restored.azurelevel').read_text())
descriptor=json.loads((source/'full-restored.azureproject').read_text())
transfer=2**(ref['renderSettings']['exposure']-near['renderSettings']['exposure'])
assert transfer==.25

def apply_reference_lighting(scene):
    result=copy.deepcopy(scene)
    preset=copy.deepcopy(ref['renderSettings']['nprLighting'])
    preset['specularFloor']=scene['renderSettings']['nprLighting']['specularFloor']
    preset['outdoorGain']*=transfer
    result['renderSettings']['nprLighting']=preset
    for light in result['lights']:
        old=light['shape'].get('receivers',{})
        light['shape']['receivers']={'schemaVersion':1,'surfaces':old.get('surfaces',True),'characters':False}
    source_nodes={n['id']:n for n in ref['nodes']}
    for light in ref['lights']:
        value=copy.deepcopy(light);old_id=light['nodeId'];identity='portrait-reference-'+light['id']
        value.update(id=identity,nodeId=identity)
        value['shape']['receivers']={'schemaVersion':1,'surfaces':False,'characters':True}
        if value['shape']['type']!='directional':value['intensity']*=transfer
        node=copy.deepcopy(source_nodes[old_id]);node.update(id=identity,parentId='hero')
        result['lights'].append(value);result['nodes'].append(node)
    return result

def hidden(scene):
    result=copy.deepcopy(scene);hero=next(n for n in result['nodes'] if n['id']=='hero')
    hero['components']['azure.renderable']['data']['visible']=False
    return result

cases={'near-control':near,'near-reference-light':apply_reference_lighting(near),
    'full-control':full,'full-reference-light':apply_reference_lighting(full),
    'hidden-control':hidden(full),'hidden-reference-light':hidden(apply_reference_lighting(full))}
rows=[]
for name,scene in cases.items():
    path=target/'scene'/(name+'.azurelevel');path.write_text(json.dumps(scene,indent=2)+'\n')
    project=copy.deepcopy(descriptor);project['id']=name;project['startupScene']='stage:/'+path.name
    dest=target/(name+'.azureproject');dest.write_text(json.dumps(project,indent=2)+'\n')
    rows.append({'name':name,'project':str(dest),'levelSha256':hashlib.sha256(path.read_bytes()).hexdigest()})
(run/'character-cases.json').write_text(json.dumps(rows,indent=2)+'\n')
(run/'character-lighting-provenance.json').write_text(json.dumps({'status':'diagnostic-not-promoted',
    'referenceProject':str(portrait),'referenceSha256':hashlib.sha256(portrait.read_bytes()).hexdigest(),
    'referenceExposureEv':ref['renderSettings']['exposure'],'targetExposureEv':near['renderSettings']['exposure'],
    'linearExposureTransfer':transfer,'mainAndAmbientGain':'reference outdoorGain multiplied by exposure transfer; shadowOverride is 1',
    'localLightIntensity':'reference local intensity multiplied by exposure transfer',
    'lightFrame':'reference local lights parented to the character transform through normal scene hierarchy',
    'environmentPolicy':'Original light surface receivers, scene geometry, GI, reflection, exposure, LUT and tone mapping retained',
    'scope':'Authored reference-lighting experiment. Complete visual acceptance, native scene equivalence and moving character quality remain pending.'},indent=2)+'\n')
print('Prepared',len(rows),'projects; reference exposure transfer',transfer,flush=True)
