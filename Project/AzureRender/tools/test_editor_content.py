"""Exercise content editing through production UI input in two projects."""
import argparse
import json
import shutil
import time
from pathlib import Path
from create_playable_project import create
from test_editor_workspace import launch


def run(executable, root, workflow):
    root.mkdir(parents=True)
    if workflow == 'exploration':
        create(root / 'game'); identity = 'hero:body'
    else:
        shutil.copytree(Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector', root / 'game',
                        ignore=shutil.ignore_patterns('.azure'))
        descriptor = json.loads((root / 'game/project.azureproject').read_text(encoding='utf-8'))
        document = json.loads((root / 'game/assets' / descriptor['startupScene'].removeprefix('assets:/')).read_text(encoding='utf-8'))
        identity = next(node['id'] for node in document['nodes'] if node.get('resourceId'))
    (root / 'game/assets/catalog-test.json').write_text('{}', encoding='utf-8')
    descriptor = json.loads((root / 'game/project.azureproject').read_text(encoding='utf-8'))
    level = root / 'game/assets' / descriptor['startupScene'].removeprefix('assets:/')
    document = json.loads(level.read_text(encoding='utf-8'))
    for node_id in ('work-a', 'work-b'):
        document['nodes'].insert(0, dict(id=node_id, name=node_id, components={
            'azure.transform': dict(type='azure.transform', version=1, data=dict(translation=[2,1,0])),
            'azure.character': dict(type='azure.character', version=1, data=dict(speed=12)),
            'azure.interactable': dict(type='azure.interactable', version=1, data=dict(prompt='Edit me', range=3))}))
    shutil.copyfile(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector/assets/cube.gltf',root/'game/assets/placement-cube.gltf')
    document['resources'].append(dict(id='placement-cube',asset='assets:/placement-cube.gltf'))
    document['nodes'].insert(0,dict(id='work-surface',name='Placement surface',resourceId='placement-cube',components={
        'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[0,4,0],scale=[3,.2,3]))}))
    level.write_text(json.dumps(document), encoding='utf-8')
    name = '长名称_' + 'abcdefghij' * 60
    events = [dict(frame=25, action='click', target='node.' + identity),
              dict(frame=29, action='click', target='name'),
              dict(frame=31, action='key', key='A', ctrl=True),
              dict(frame=32, action='key', key='A', ctrl=True, down=False),
              dict(frame=33, action='key', key='A', down=False),
              dict(frame=34, action='text', text=name[:200]),
              dict(frame=35, action='text', text=name[200:400]),
              dict(frame=36, action='text', text=name[400:]),
              dict(frame=37, action='key', key='Enter'),
              dict(frame=38, action='key', key='Enter', down=False),
              dict(frame=41, action='click', target='save')]
    data = launch(executable, root, 'long-name', (1920, 1080), 1, events, capture=False)
    assert data['undoCount']==1, 'Several text events must form one undo unit while panel queries continue'
    assert data['selectedName'] == name, 'Details must commit the complete UTF-8 name beyond the former fixed buffer'
    reopened = launch(executable, root, 'reopen', (1920, 1080), 1,
                      [dict(frame=25, action='click', target='node.' + identity)], capture=False, close_policy='discard')
    assert reopened['selectedName'] == name, 'Long names must survive save and reopen'
    types = launch(executable, root, 'registered-types', (1920, 1080), 1,
                   [dict(frame=25, action='click', target='tool.assets'),
                    dict(frame=29, action='click', target='assets.type'),
                    dict(frame=33, action='click', target='type.Animation graphs / JSON')], capture=False)
    assert types['visibleAssets'] and all(row['type'] == 'animation' for row in types['visibleAssets']), 'Registered animation assets must be available through the browser filter'
    drag = [dict(frame=25, action='mouse', target='node.work-a', down=True),
            dict(frame=28, action='mouse', target='node.work-a', down=True, offset=[8,0]),
            dict(frame=32, action='mouse', target='node.work-b', down=True),
            dict(frame=36, action='mouse', target='node.work-b', down=False)]
    moved = launch(executable,root,'reparent',(1920,1080),1,drag,capture=False)
    authored=json.loads(level.read_text(encoding='utf-8'))
    assert next(n for n in authored['nodes'] if n['id']=='work-a')['parentId']=='work-b', 'Outliner drop must reparent through the production operation'
    root_drop = [dict(frame=25,action='click',target='node.work-b',offset=[-139,0]),
                 dict(frame=29,action='mouse',target='node.work-a',down=True),
                 dict(frame=32,action='mouse',target='node.work-a',down=True,offset=[8,0]),
                 dict(frame=36,action='mouse',target='hierarchy.root',down=True),
                 dict(frame=40,action='mouse',target='hierarchy.root',down=False)]
    launch(executable,root,'root-drop',(1920,1080),1,root_drop,capture=False)
    authored=json.loads(level.read_text(encoding='utf-8'))
    assert not next(n for n in authored['nodes'] if n['id']=='work-a').get('parentId'), 'Root drop must preserve the world transform'
    prompt='组件长文本_'+'abcdefghij'*60
    editing=[dict(frame=25,action='click',target='node.work-a'),
             dict(frame=27,action='click',target='component.azure.transform'),
             dict(frame=29,action='click',target='component.azure.interactable'),
             dict(frame=33,action='click',target='field.azure.interactable.prompt'),
             dict(frame=35,action='key',key='A',ctrl=True),dict(frame=36,action='key',key='A',ctrl=False,down=False),
             dict(frame=37,action='text',text=prompt),dict(frame=40,action='key',key='Enter'),dict(frame=41,action='key',key='Enter',down=False)]
    launch(executable,root,'long-component',(1920,1080),1,editing,capture=False)
    authored=json.loads(level.read_text(encoding='utf-8'))
    node=next(n for n in authored['nodes'] if n['id']=='work-a')
    assert node['components']['azure.interactable']['data']['prompt']==prompt, 'Component strings must preserve full UTF-8 content'
    reset=[dict(frame=25,action='click',target='node.work-a'),dict(frame=27,action='click',target='component.azure.transform'),dict(frame=29,action='click',target='component.azure.character'),
           dict(frame=33,action='click',target='field.azure.character.speed',button='right'),
           dict(frame=37,action='click',target='field.reset.azure.character.speed')]
    launch(executable,root,'reset-field',(1920,1080),1,reset,capture=False)
    authored=json.loads(level.read_text(encoding='utf-8'))
    assert next(n for n in authored['nodes'] if n['id']=='work-a')['components']['azure.character']['data']['speed']==4, 'Field menu must use registered defaults'
    remove=[dict(frame=25,action='click',target='node.work-a'),dict(frame=29,action='click',target='component.azure.character',button='right'),
            dict(frame=33,action='click',target='component.remove.azure.character')]
    launch(executable,root,'remove-component',(1920,1080),1,remove,capture=False)
    authored=json.loads(level.read_text(encoding='utf-8'))
    assert 'azure.character' not in next(n for n in authored['nodes'] if n['id']=='work-a')['components'], 'Component removal must update saved content'
    source=next((root/'game/assets').rglob('*.gltf')).resolve()
    import_events=[dict(frame=25,action='click',target='tool.assets'),dict(frame=29,action='click',target='assets.import-create'),
                   dict(frame=33,action='click',target='import.path'),dict(frame=37,action='text',text=str(source)),
                   dict(frame=41,action='click',target='asset.import'),dict(frame=45,action='click',target='menu.View'),
                   dict(frame=49,action='click',target='panel.assets')]
    imported=launch(executable,root,'closed-import',(1920,1080),1,import_events,capture=False)
    assert not imported['panels']['assets']['open'] and imported['tasks'][-1]['state']=='completed', 'Import must finish with the content panel closed'
    failing=import_events[:3]+[dict(frame=37,action='text',text=str(root/'missing.gltf')),dict(frame=41,action='click',target='asset.import')]
    failed=launch(executable,root,'import-error',(1920,1080),1,failing,capture=False)
    assert failed['feedback'] and failed['tasks'][-1]['state']=='failed', 'Background error must stay visible after unrelated UI work'
    feedback_id=failed['feedback'][-1]['id']
    dismissed=launch(executable,root,'dismiss-error',(1920,1080),1,failing+[
        dict(frame=49,action='click',target='tool.console'),dict(frame=53,action='click',target='feedback.dismiss.'+feedback_id)],capture=False)
    assert not dismissed['feedback'], 'Console dismissal must close only the selected error record'
    asset_id=json.loads((root/'game/assets/placement-cube.gltf.azmeta').read_text(encoding='utf-8'))['id']
    before_nodes=json.loads(level.read_text(encoding='utf-8'))['nodes'];before_ids={node['id'] for node in before_nodes}
    placement=[dict(frame=25,action='click',target='node.work-surface'),dict(frame=27,action='click',target='node.work-surface'),
               dict(frame=33,action='click',target='tool.assets'),dict(frame=37,action='click',target='assets.search'),dict(frame=39,action='text',text='placement-cube'),
               dict(frame=45,action='mouse',target='asset.'+asset_id,down=True),dict(frame=48,action='mouse',target='asset.'+asset_id,down=True,offset=[8,0]),
               dict(frame=52,action='mouse',target='pick.work-surface',down=True),dict(frame=56,action='mouse',target='pick.work-surface',down=False)]
    placed=launch(executable,root,'surface-placement',(1920,1080),1,placement,capture=False)
    assert placed['camera']['target'][1]>3.5, 'Outliner double click must frame the elevated surface before placement'
    added=[node for node in json.loads(level.read_text(encoding='utf-8'))['nodes'] if node['id'] not in before_ids]
    assert len(added)==1 and added[0]['components']['azure.transform']['data']['translation'][1]>3.5, 'Viewport drop must hit elevated geometry rather than the fallback floor'
    assert not placed['feedback'], 'Scene placement must save and close without feedback errors'
    summary = dict(workflow=workflow, longName=True, saveReopen=True, registeredTypes=True, reparent=True, rootDrop=True, longComponent=True, reset=True, remove=True, closedImport=True, errorDismiss=True, geometryPlacement=True, passed=True)
    (root / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve() / ('run-' + str(time.time_ns()))
    summaries = [run(args.executable.resolve(), root / workflow, workflow) for workflow in ('exploration', 'inspector')]
    (root / 'summary.json').write_text(json.dumps(summaries, indent=2), encoding='utf-8')
    print(json.dumps(summaries))
