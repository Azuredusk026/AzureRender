"""Cover creation shortcuts and coordinate-space editing in both projects."""
import json
from pathlib import Path
import shutil
import sys
import time

ROOT=Path.cwd().resolve();sys.path.insert(0,str(ROOT/'tools'))
from create_playable_project import create
from test_editor_workspace import launch
OUT=ROOT/'build/p4/workflow-supplement'/('run-'+str(time.time_ns()))
EXE=ROOT/'build/ninja-msvc-release/AzureRender.exe'
summaries=[]
for workflow in ('exploration','inspector'):
    root=OUT/workflow;root.mkdir(parents=True)
    if workflow=='exploration':create(root/'game')
    else:shutil.copytree(ROOT/'assets_public/scene_inspector',root/'game',ignore=shutil.ignore_patterns('.azure'))
    descriptor=json.loads((root/'game/project.azureproject').read_text(encoding='utf-8'))
    level=root/'game/assets'/descriptor['startupScene'].removeprefix('assets:/')
    original=json.loads(level.read_text(encoding='utf-8'))
    events=[dict(frame=25,action='click',target='create.empty'),dict(frame=27,action='click',target='focus'),
        dict(frame=29,action='key',key='D',ctrl=True),dict(frame=30,action='key',key='D',ctrl=True,down=False),
        dict(frame=34,action='key',key='Delete'),dict(frame=35,action='key',key='Delete',down=False),
        dict(frame=39,action='key',key='Z',ctrl=True),dict(frame=40,action='key',key='Z',ctrl=True,down=False)]
    result=launch(EXE,root,'creation',(1920,1080),1,events,capture=False)
    authored=json.loads(level.read_text(encoding='utf-8'))
    added=[n for n in authored['nodes'] if n['id'] not in {n['id'] for n in original['nodes']}]
    assert len(added)==2 and len({n['id'] for n in authored['nodes']})==len(authored['nodes'])
    assert result['undoCount']==2,result
    fixture=json.loads(json.dumps(original))
    fixture['nodes'].insert(0,dict(id='space-target',name='Space target',components={
        'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[0,0,0],rotation=[0,0,90],scale=[1,1,1]))}))
    for space,offset,axis in (('world',[100,0],0),('local',[0,-100],1)):
        level.write_text(json.dumps(fixture),encoding='utf-8')
        start=[dict(frame=25,action='click',target='node.space-target'),dict(frame=28,action='click',target='focus'),
            dict(frame=31,action='click',target='mode.Move'),dict(frame=34,action='click',target='gizmo.options'),
            dict(frame=37,action='click',target='gizmo.space.'+space)]
        events=start+[dict(frame=42,action='mouse',target='gizmo.0',down=True),
            dict(frame=45,action='mouse',target='gizmo.0',down=True,offset=offset),
            dict(frame=48,action='mouse',target='gizmo.0',down=False,offset=offset)]
        result=launch(EXE,root,'space-'+space,(1920,1080),1,events,capture=False)
        position=result['gizmoTranslation']
        assert abs(position[axis])>.001 and all(abs(v)<.001 for i,v in enumerate(position) if i!=axis),(space,position)
        assert result['undoCount']==1 and not result['capture']['gizmo']
        level.write_text(json.dumps(fixture),encoding='utf-8')
        undo=events+[dict(frame=52,action='key',key='Z',ctrl=True),dict(frame=53,action='key',key='Z',ctrl=True,down=False)]
        result=launch(EXE,root,'undo-'+space,(1920,1080),1,undo,capture=False)
        assert all(abs(v)<.001 for v in result['gizmoTranslation']) and result['undoCount']==0
    child=dict(id='space-child',name='Space child',parentId='space-target',components={
        'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[1,0,0],rotation=[0,0,0],scale=[1,1,1]))})
    fixture['nodes'].insert(1,child)
    level.write_text(json.dumps(fixture),encoding='utf-8')
    multi=[dict(frame=25,action='click',target='outliner.search'),dict(frame=28,action='text',text='Space'),
        dict(frame=31,action='click',target='node.space-target'),dict(frame=34,action='click',target='node.space-child',ctrl=True),
        dict(frame=37,action='click',target='focus'),dict(frame=40,action='click',target='mode.Move'),
        dict(frame=43,action='mouse',target='gizmo.0',down=True),dict(frame=46,action='mouse',target='gizmo.0',down=True,offset=[100,0]),
        dict(frame=49,action='mouse',target='gizmo.0',down=False,offset=[100,0])]
    result=launch(EXE,root,'multi-parent-child',(1920,1080),1,multi,capture=False)
    authored=json.loads(level.read_text(encoding='utf-8'))
    parent=next(n for n in authored['nodes'] if n['id']=='space-target')
    child=next(n for n in authored['nodes'] if n['id']=='space-child')
    assert abs(parent['components']['azure.transform']['data']['translation'][0])>.001
    assert child['components']['azure.transform']['data']['translation']==[1,0,0]
    assert result['undoCount']==1 and set(result['selection'])=={'space-target','space-child'}
    level.write_text(json.dumps(fixture),encoding='utf-8')
    result=launch(EXE,root,'multi-undo',(1920,1080),1,multi+[
        dict(frame=53,action='key',key='Z',ctrl=True),dict(frame=54,action='key',key='Z',ctrl=True,down=False)],capture=False)
    assert result['undoCount']==0
    restored=json.loads(level.read_text(encoding='utf-8'))
    for identity,expected in (('space-target',[0,0,0]),('space-child',[1,0,0])):
        node=next(n for n in restored['nodes'] if n['id']==identity)
        assert node['components']['azure.transform']['data']['translation']==expected
    summaries.append(dict(workflow=workflow,passed=True,createDuplicateDeleteUndo=True,worldLocal=True,multiParentChild=True,singleUndo=True,inputSource='Dear ImGui production input'))
(OUT/'summary.json').write_text(json.dumps(summaries,indent=2),encoding='utf-8');print(json.dumps(summaries))
