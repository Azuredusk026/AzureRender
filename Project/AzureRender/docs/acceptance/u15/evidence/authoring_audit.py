"""Retain two-template production authoring tasks, inputs and content hashes."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import time
from PIL import Image

ROOT=Path.cwd().resolve();sys.path.insert(0,str(ROOT/'tools'))
from editor_test_host import EditorHost as BaseHost

class EditorHost(BaseHost):
    def __init__(self,*args):
        super().__init__(*args);self.audit=[];self.started=time.monotonic()
    def call(self,op,passed=True,**fields):
        begin=time.monotonic()
        try:return super().call(op,passed=passed,**fields)
        finally:self.audit.append(dict(op=op,command=fields.get('commandId'),
            seconds=time.monotonic()-begin,expectedSuccess=passed,fields=fields))
    def __exit__(self,*args):
        try:return super().__exit__(*args)
        finally:
            summary=dict(input='production ImGui replay and public edit operations',
                elapsedSeconds=time.monotonic()-self.started,physicalInput=False,
                steps=sum(row['op'] in ('edit','input') for row in self.audit),
                injectedFailures=sum(not row['expectedSuccess'] for row in self.audit),
                operations=self.audit)
            (self.output/'audit.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


OUT=ROOT/'build/p5-p6-closure-2026-10-10/authoring-audit'
OUT.mkdir(parents=True,exist_ok=False)
EXE=ROOT/'build/ninja-msvc-release/AzureRender.exe'
seed=OUT/'seed'
shutil.copytree(ROOT/'assets_public/scene_inspector',seed,ignore=shutil.ignore_patterns('.azure'))
source=OUT/'model-input';source.mkdir()
mesh=json.loads((ROOT/'assets_public/scene_inspector/assets/cube.gltf').read_text(encoding='utf-8'))
second=json.loads(json.dumps(mesh['materials'][0]));second['name']='second-surface'
mesh['materials'].append(second);mesh['meshes'][0]['primitives'][1]['material']=1
mesh['images']=[{'uri':'source.png'}];mesh['textures']=[{'source':0}]
mesh['materials'][0]['pbrMetallicRoughness']['baseColorTexture']={'index':0}
(source/'model.gltf').write_text(json.dumps(mesh,indent=2)+'\n',encoding='utf-8')
Image.new('RGBA',(32,32),(120,145,190,255)).save(source/'source.png')
rows=[]
def fingerprints(project):
    return {p.relative_to(project).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(project.rglob('*')) if p.is_file() and '.azure' not in p.parts}

for template,workflow in [('game','exploration'),('scene','inspection')]:
    project=OUT/workflow
    with EditorHost(EXE,seed/'project.azureproject',OUT/(workflow+'-create')) as host:
        host.edit('project.create',templateId=template,destination=str(project),name='P5 '+workflow)
        deadline=time.monotonic()+45
        while host.workspace()['project']['name']!='P5 '+workflow:
            assert time.monotonic()<deadline
            host.frames()
        assert host.workspace()['undoCount']==0
        host.screenshot('p5-'+workflow+'-created')
    descriptor=json.loads((project/'project.azureproject').read_text(encoding='utf-8'))
    level=project/'assets'/descriptor['startupScene'].split(':/',1)[1]
    assert level.suffix=='.azurelevel'
    with EditorHost(EXE,project/'project.azureproject',OUT/(workflow+'-authoring')) as host:
        initial=host.query('document.contentHash')
        imported=host.edit('asset.import',path=str(source/'model.gltf'))
        catalog=host.edit('assets.catalog')
        texture=next(r for r in catalog if r['type']=='image' and r['path'].endswith('source.png'))['id']
        host.edit('document.save')
        host.edit('content.create-directory',parent='assets:/',name='Authored')
        host.edit('asset.move',asset=imported,destination='assets:/Authored/model.gltf')
        host.edit('asset.rename',asset=imported,name='Mechanical.gltf')
        assert host.edit('asset.inspect',asset=imported)['id']==imported
        host.edit('texture.settings',assets=[texture],values={'filter':'linear','usage':'albedo'})
        host.edit('node.place',resource=imported,id='authored-model')
        host.edit('document.save')
        for type_id,name in [('azure.pbr','Metal'),('azure.stylized','Style')]:
            material=host.edit('material.create',type=type_id,destination='assets:/Authored/'+name+'.azurematerial')['asset']
            host.edit('material.field',asset=material,field='roughness',value=.4)
            slot='name:stone' if name=='Metal' else 'name:second-surface'
            host.edit('material.assign',nodes=['authored-model'],slotId=slot,material=material)
            host.edit('reference.reveal',kind='asset',id=material)
        primitive=host.edit('asset.generate',generator='azure.primitive',output='assets:/Authored/Cube.glb',
            parameters={'kind':'cube','size':[1,1,1],'segments':1},license='CC0-1.0',replace=False)
        host.edit('node.place',resource=primitive,id='authored-fixed')
        host.edit('node.place',resource=primitive,id='authored-moving')
        host.edit('component.batch-field',nodes=['authored-fixed','authored-moving'],type='azure.transform',field='scale',value=[.5,.5,.5])
        host.edit('node.create-preset',preset='light',id='authored-light')
        host.edit('node.create-preset',preset='camera',id='authored-camera')
        host.edit('reference.begin',nodes=['authored-camera'],type='azure.third-person-camera',field='target')
        host.click('node.authored-model')
        assert not host.workspace()['referencePicker']
        host.edit('node.select',id='authored-model')
        reusable=host.edit('prefab.create',nodes=['authored-model'],path='assets:/Authored/Mechanism.azureprefab')['asset']
        for instance,name in [('first','First override'),('second','Second override')]:
            host.edit('prefab.place',asset=reusable,instance=instance)
            host.edit('node.select',id=instance+':authored-model');host.edit('node.rename',value=name)
            overrides=host.edit('prefab.overrides',instance=instance)
            assert any(f['node']=='authored-model' and f['path']=='/name' for f in overrides['fields'])
        for node,dynamic,position in [('authored-fixed',False,[5,3,0]),('authored-moving',True,[5,2,0])]:
            host.edit('node.select',id=node)
            host.edit('component.fields',type='azure.transform',values={'translation':position})
            host.edit('component.add',type='azure.rigid-body')
            host.edit('component.fields',type='azure.rigid-body',values={'dynamic':dynamic,'shape':'box','halfExtent':[.25,.25,.25]})
        host.edit('node.create',id='authored-joint');host.edit('component.add',type='azure.hinge')
        host.edit('component.fields',type='azure.hinge',values={'bodyA':'authored-fixed','bodyB':'authored-moving'})
        host.edit('document.save');authored=host.query('document.contentHash')
        host.click('simulate');host.frames(30)
        assert host.workspace()['previewMode']=='simulate' and host.workspace()['physicsConstraints']>=1
        host.click('pause');steps=host.workspace()['previewSteps'];host.click('step')
        assert host.workspace()['previewSteps']==steps+1
        host.click('stop');assert host.query('document.contentHash')==authored
        host.edit('node.select',id='authored-model')
        script=host.edit('script.template',backend='lua',path='assets:/Authored/Controller.lua')['asset']
        host.edit('script.write',path='assets:/Authored/Controller.lua',value='function init() end\nfunction update(dt) end\n')
        host.edit('component.add',type='azure.script');host.edit('component.field',type='azure.script',field='asset',value=script)
        host.edit('component.field',type='azure.script',field='enabled',value=False)
        host.edit('component.field',type='azure.script',field='enabled',value=True)
        host.edit('component.field',passed=False,type='azure.transform',field='translation',value='invalid')
        problems=host.edit('diagnostic.query',source='component.field')['entries'];assert problems[-1]['node']=='authored-model'
        host.click('tool.problems');host.click('problem.'+problems[-1]['id'])
        assert host.workspace()['selection']==['authored-model']
        host.edit('document.save')
        first=host.workspace()['document']['documentId']
        second=project/'assets/second.azurelevel';shutil.copy2(level,second)
        host.edit('assets.reload');host.frames()
        host.edit('document.open',path='assets:/second.azurelevel');host.frames()
        host.edit('node.select',id='authored-model');host.edit('node.rename',value='Second document')
        host.click('save');second_id=host.workspace()['document']['documentId']
        host.edit('document.activate',id=first);host.frames()
        assert host.workspace()['document']['documentId']==first and len(host.workspace()['documents'])==2
        host.screenshot('p5-'+workflow+'-authored')
        host.edit('document.activate',id=second_id);host.frames()
        assert host.workspace()['selectedName']=='Second document'
    saved=json.loads(level.read_text(encoding='utf-8'))
    assert len(saved['prefabs'])==2
    assert {i['overrides']['authored-model']['name'] for i in saved['prefabs']}=={'First override','Second override'}
    assert next(n for n in saved['nodes'] if n['id']=='authored-camera')['components']['azure.third-person-camera']['data']['target']=='authored-model'
    hashes=fingerprints(project)
    with EditorHost(EXE,project/'project.azureproject',OUT/(workflow+'-reopen')) as host:
        host.edit('node.select',id='authored-moving')
        assert host.workspace()['selection']==['authored-moving']
        host.click('simulate');assert host.workspace()['physicsConstraints']>=1
        host.click('stop')
    backups=[p for p in (project/'.azure/recovery').glob('*-'+level.name) if p.is_file()]
    assert backups,'Save must provide a recoverable content copy'
    backup=max(backups,key=lambda p:p.stat().st_mtime)
    recovery=OUT/(workflow+'-recovery.json')
    recovery.write_text(json.dumps(dict(source=str(level),backup=str(backup),backupSha256=hashlib.sha256(backup.read_bytes()).hexdigest(),
        currentSha256=hashlib.sha256(level.read_bytes()).hexdigest()),indent=2)+'\n',encoding='utf-8')
    original=level.read_bytes();level.write_bytes(backup.read_bytes())
    try:
        with EditorHost(EXE,project/'project.azureproject',OUT/(workflow+'-recovered')) as host:
            assert host.workspace()['nodeCount']>0
    finally:level.write_bytes(original)
    assert fingerprints(project)==hashes,'Reopen and backup verification must preserve authored content'
    rows.append(dict(workflow=workflow,template=template,passed=True,project=str(project),input='production UI and versioned public operations',
        created=True,importedModelAndTexture=True,moveRenameIdentity=imported,materials=2,submeshSlots=2,
        primitives=True,lightAndCamera=True,multiEdit=True,objectEyedropper=True,assetReveal=True,
        independentPrefabOverrides=2,physicsMechanism=True,scriptFields=True,diagnosticLocation=True,
        savedDocuments=2,reopen=True,backupRecovery=True,contentHashes=hashes,physicalInput='pending'))
    (OUT/'partial.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    print(workflow,'authoring verified',flush=True)
(OUT/'summary.json').write_text(json.dumps(dict(passed=True,workflows=rows,animationEvidence='full CTest AuthoringGpu and AnimationAuthoring'),indent=2)+'\n',encoding='utf-8')
