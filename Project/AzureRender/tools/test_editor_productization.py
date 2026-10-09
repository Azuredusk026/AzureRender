"""Validate project and workspace controls using production Dear ImGui input."""
import argparse
import json
import tempfile
import shutil
import uuid
from pathlib import Path
from create_playable_project import create
from test_editor_workspace import launch


def run(executable, output):
    output.mkdir(parents=True, exist_ok=True)
    if (output/"game").exists():
        output=output/("run-"+uuid.uuid4().hex[:8]);output.mkdir()
    create(output / 'game')
    events = [dict(frame=25, action='click', target='menu.View'),
              dict(frame=29, action='click', target='layout.authoring'),
              dict(frame=36, action='click', target='menu.File'),
              dict(frame=40, action='click', target='menu.Projects')]
    data = launch(executable, output, 'project-entry', (1920, 1080), 1, events)
    assert data['preset'] == 'authoring', data['preset']
    assert data['panels']['projects']['open'], data['panels']['projects']
    assert data['panels']['environment']['open'], data['panels']['environment']
    assert not data['panels']['capture']['open']
    assert 'project.open.path' in data['widgets'], data['widgets'].keys()
    assert 'project.create' in data['widgets'], data['widgets'].keys()
    assert 'project.recent.'+data['project']['id'] in data['widgets'], 'Startup project must be recorded in recents'
    restored = launch(executable, output, 'restore', (1920, 1080), 1,
                      capture=False, config=output/'project-entry/config')
    assert restored['preset'] == 'authoring'
    debug = launch(executable, output, 'debug-layout', (1280, 720), 1,
                   [dict(frame=25, action='click', target='menu.View'),
                    dict(frame=29, action='click', target='layout.debugging')], capture=False)
    assert debug['preset'] == 'debugging'
    assert debug['panels']['gameplay-debug']['open']
    assert debug['panels']['capture']['open']
    second=output/'second'
    shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector',second,ignore=shutil.ignore_patterns('.azure'))
    expected=json.loads((second/'project.azureproject').read_text(encoding='utf-8'))
    switch_events=[dict(frame=25,action='click',target='menu.File'),dict(frame=29,action='click',target='menu.Projects'),
        dict(frame=36,action='click',target='project.open.path'),dict(frame=39,action='key',key='A',down=True,ctrl=True),
        dict(frame=40,action='key',key='A',down=False,ctrl=True),dict(frame=41,action='key',key='A',down=False,ctrl=False),
        dict(frame=43,action='text',text=str(second/'project.azureproject')),dict(frame=50,action='click',target='project.open')]
    switched=launch(executable,output,'project-switch',(1920,1080),1,switch_events,capture=False)
    assert switched['project']['id']==expected['id'],switched.get('project')
    assert switched['undoCount']==0
    assert switched['feedback']==[],switched['feedback']
    matrix=[]
    for workflow in ('exploration','inspection'):
        folder=output/('matrix-'+workflow);folder.mkdir()
        if workflow=='exploration':
            create(folder/'game')
        else:
            shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector',folder/'game',ignore=shutil.ignore_patterns('.azure'))
        for size,dpi in (((1280,720),1),((1920,1080),1),((2560,1440),1),((1920,1080),1.5),((2560,1440),1.5),((2560,1440),2)):
            case=f'{size[0]}x{size[1]}-{dpi}'
            result=launch(executable,folder,case,size,dpi,
                [dict(frame=25,action='click',target='menu.View'),dict(frame=29,action='click',target='layout.authoring')],capture=False)
            assert result['preset']=='authoring'
            for panel in ('viewport','outliner','inspector','assets','environment'):
                assert result['panels'][panel]['open'] and result['panels'][panel]['docked'],(workflow,case,panel)
            matrix.append(dict(workflow=workflow,size=size,dpi=dpi))
    summary = dict(passed=True, stage='U8', projectEntry=True, projectSwitch=True,
                   authoringPreset=True, debuggingPreset=True, matrix=matrix,
                   presetPersistence=True, input='production Dear ImGui replay',
                   physicalKeyboardMouse='not performed')
    (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_productization_') as temp:
        run(args.executable.resolve(), args.output.resolve() if args.output else Path(temp))
