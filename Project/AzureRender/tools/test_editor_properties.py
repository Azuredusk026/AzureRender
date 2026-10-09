"""Exercise property resets and reference picking through real UI input."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from test_editor_workspace import launch


def run(executable, root):
    if root.exists():root=root/("run-"+uuid.uuid4().hex[:8])
    root.mkdir(parents=True, exist_ok=True)
    shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector', root/'game', ignore=shutil.ignore_patterns('.azure'))
    level=root/'game/assets/inspection.azurelevel'
    document=json.loads(level.read_text(encoding='utf-8'))
    document['nodes'][0]['components']['azure.transform']['data']['translation']=[1,2,3]
    document['nodes'][0]['components']['azure.third-person-camera']={'type':'azure.third-person-camera','version':1,'data':{'target':'observer'}}
    level.write_text(json.dumps(document,indent=2)+'\n',encoding='utf-8')
    actions=[dict(frame=25,action='click',target='node.observer'),
             dict(frame=31,action='click',target='axis.reset.azure.transform.translation.0')]
    data=launch(executable,root,'axis-reset',(1920,1080),1,actions)
    assert data['gizmoTranslation'][0]==0, data['gizmoTranslation']
    assert data['undoCount']==1, data['undoCount']
    assert data['feedback']==[], data['feedback']
    assert 'field.azure.transform.rotation' in data['widgets']
    assert data['gizmoTranslation']==[0,2,3], data['gizmoTranslation']
    pick=[dict(frame=25,action='click',target='node.observer'),
          dict(frame=31,action='click',target='component.azure.third-person-camera'),
          dict(frame=36,action='click',target='reference.pick.azure.third-person-camera.target'),
          dict(frame=42,action='click',target='node.asset-b')]
    reference=launch(executable,root,'reference-pick',(1920,1080),1,pick,capture=False)
    assert reference['selection']==['observer'], reference['selection']
    assert not reference['referencePicker']
    assert reference['undoCount']==1, reference['undoCount']
    assert reference['feedback']==[], reference['feedback']
    saved=json.loads(level.read_text(encoding='utf-8'))
    assert saved['nodes'][0]['components']['azure.third-person-camera']['data']['target']=='asset-b'
    restored=launch(executable,root,'saved-reference',(1920,1080),1,capture=False)
    assert restored['undoCount']==0
    cancel=launch(executable,root,'cancel-pick',(1920,1080),1,pick[:-1]+[
        dict(frame=42,action='key',key='Escape',down=True),dict(frame=43,action='key',key='Escape',down=False)],capture=False)
    assert not cancel['referencePicker'] and cancel['undoCount']==0
    focus=launch(executable,root,'focus-cancel',(1920,1080),1,pick[:-1]+[dict(frame=42,action='focus',focused=False)],capture=False)
    assert not focus['referencePicker'] and focus['undoCount']==0
    multi=launch(executable,root,'mixed-reset',(1920,1080),1,[
        dict(frame=25,action='click',target='node.asset-a'),
        dict(frame=29,action='key',key='A',down=False,ctrl=True),
        dict(frame=31,action='click',target='node.asset-b',ctrl=True),
        dict(frame=34,action='key',key='A',down=False,ctrl=False),
        dict(frame=39,action='click',target='axis.reset.azure.transform.translation.0')],capture=False)
    assert multi['selection']==['asset-a','asset-b'],multi['selection']
    assert multi['undoCount']==1,multi['undoCount']
    saved=json.loads(level.read_text(encoding='utf-8'))
    transforms={n['id']:n['components']['azure.transform']['data']['translation'] for n in saved['nodes']}
    assert transforms['asset-a']==[0,0,-1] and transforms['asset-b']==[0,0,-1],transforms
    from create_playable_project import create
    second=root/'exploration';second.mkdir();create(second/'game')
    playable=launch(executable,second,'properties',(1920,1080),1,[dict(frame=25,action='click',target='node.hero:body')],capture=False)
    assert 'field.azure.transform.translation' in playable['widgets']
    assert playable['feedback']==[]
    summary=dict(passed=True,stage='U9',axisReset=True,referencePick=True,referenceSaveReopen=True,escapeCancel=True,focusCancel=True,mixedReset=True,secondProject=True,input='production Dear ImGui replay',physicalKeyboardMouse='not performed')
    (root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary))

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    run(args.executable.resolve(),args.output.resolve())
