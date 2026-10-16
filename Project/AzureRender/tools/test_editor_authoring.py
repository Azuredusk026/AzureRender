"""Verify authoring panel discovery and real ImGui controls."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from test_editor_workspace import launch
from editor_test_host import EditorHost

def retained_workflows(executable,root):
    with EditorHost(executable,root/'game/project.azureproject',root/'animation-controls') as host:
        original=host.query('document.contentHash')
        host.edit('node.select',id='hero');host.click('focus');host.click('tool.animation')
        host.edit('animation.seek',node='hero',state='walk',time=.25);host.frames()
        host.click('animation.speed',offset=[30,0]);assert host.workspace()['animationTimelines']['hero']['speed']>1
        # Scroll the actual bottom panel before clicking controls below its fold.
        host.call('input',event=dict(action='mouse',target='panelrect.animation',down=False));host.frames()
        host.call('input',event=dict(action='wheel',y=-4));host.frames()
        host.click('animation.loop');assert not host.workspace()['animationTimelines']['hero']['loop']
        host.click('animation.skeleton')
        assert host.workspace()['viewportOverlayPolicies']['skeleton']=='selected'
        host.screenshot('u13-skeleton-preview')
        assert host.query('document.contentHash')==original and not host.workspace()['dirty']
    source=Path(__file__).resolve().parents[1]/'assets_public/scene_inspector'
    shutil.copytree(source,root/'retained-project',ignore=shutil.ignore_patterns('.azure'))
    project=root/'retained-project';scene=project/'assets/inspection.azurelevel'
    shutil.copyfile(scene,project/'assets/second.azurelevel')
    with EditorHost(executable,project/'project.azureproject',root/'retained') as host:
        first=host.workspace()['document']['documentId']
        host.edit('document.open',path='assets:/second.azurelevel');host.frames()
        second=host.workspace()['document']['documentId'];assert first!=second
        host.edit('node.select',id='asset-a');host.edit('node.rename',value='Second scene edit');host.frames()
        host.click('document.tab.inspection.azurelevel');assert host.workspace()['document']['documentId']==first
        host.click('document.tab.second.azurelevel');assert host.workspace()['selectedName']=='Second scene edit'
        def close_tab():
            tab=host.workspace()['widgets']['document.tab.second.azurelevel'];offset=[tab[2]/2-15,0]
            host.call('input',event=dict(action='mouse',target='document.tab.second.azurelevel',offset=offset,down=False));host.frames()
            host.click('document.tab.second.azurelevel',offset=offset)
        close_tab();assert 'document.close.cancel' in host.workspace()['widgets']
        host.click('document.close.cancel');assert len(host.workspace()['documents'])==2
        # Exact component failure location, then real Problems-panel reveal.
        host.edit('component.field',passed=False,type='azure.transform',field='translation',value='invalid')
        problems=host.edit('diagnostic.query',source='component.field')['entries'];problem=problems[-1];assert problem['node']=='asset-a'
        host.edit('document.activate',id=first);host.frames();host.click('tool.problems');host.click('problem.'+problem['id'])
        assert host.workspace()['document']['documentId']==second and host.workspace()['selection']==['asset-a']
        host.screenshot('u13-problem-location')
        close_tab();host.click('document.close.save')
        assert len(host.workspace()['documents'])==1
        assert next(n for n in json.loads((project/'assets/second.azurelevel').read_text())['nodes'] if n['id']=='asset-a')['name']=='Second scene edit'
    source=Path(__file__).resolve().parents[1]/'assets_public/physics_door'
    shutil.copytree(source,root/'prefab-project',ignore=shutil.ignore_patterns('.azure'))
    project=root/'prefab-project'
    with EditorHost(executable,project/'project.azureproject',root/'prefab') as host:
        host.edit('selection.click',id='fixed');host.edit('selection.click',id='moving',ctrl=True);host.edit('selection.click',id='joint',ctrl=True)
        host.click('tool.prefabs');host.click('prefab.create')
        assert (project/'assets/NewPrefab.azureprefab').is_file()
        host.edit('prefab.place',asset='assets:/NewPrefab.azureprefab',instance='door-copy');host.edit('node.select',id='door-copy:moving')
        host.edit('node.rename',value='Reusable door');host.frames()
        host.click('prefab.field.moving/name');host.click('prefab.revert');assert host.workspace()['selectedName']=='moving'
        host.edit('history.undo');host.frames();host.click('prefab.field.moving/name');host.click('prefab.apply')
        assert next(n for n in json.loads((project/'assets/NewPrefab.azureprefab').read_text())['nodes'] if n['id']=='moving')['name']=='Reusable door'
        host.screenshot('u13-prefab-source')
        # Source history controls sit below the fold in the bottom dock.
        host.call('input',event=dict(action='mouse',target='panelrect.prefabs',down=False));host.frames()
        host.call('input',event=dict(action='wheel',y=-4));host.frames()
        host.click('prefab.undo-apply');assert next(n for n in json.loads((project/'assets/NewPrefab.azureprefab').read_text())['nodes'] if n['id']=='moving')['name']=='moving'
        host.click('prefab.redo-apply');assert next(n for n in json.loads((project/'assets/NewPrefab.azureprefab').read_text())['nodes'] if n['id']=='moving')['name']=='Reusable door'

def run(executable, output):
    root=output/('run-'+uuid.uuid4().hex[:8]);root.mkdir(parents=True)
    source=Path(__file__).resolve().parents[1]/'assets_public/third_person/game'
    shutil.copytree(source,root/'game',ignore=shutil.ignore_patterns('.azure'))
    actions=[dict(frame=20,action='click',target='tool.assistance')]
    data=launch(executable,root,'assistance',(1920,1080),1,actions,capture=True)
    assert data['panels']['assistance']['open']
    assert 'assistance.unavailable' in data['widgets'], 'Optional model service leaves an explicit manual authoring explanation'
    actions=[dict(frame=20,action='click',target='tool.scripts'),dict(frame=24,action='click',target='script.create')]
    scripts=launch(executable,root,'scripts',(1920,1080),1,actions,capture=True)
    assert scripts['panels']['scripts']['open']
    assert (root/'game/assets/NewBehaviour.lua').is_file()
    assert not scripts['dirty'] and scripts['undoCount']==0
    assert 'script.reload' in scripts['widgets']
    actions=[dict(frame=20,action='click',target='tool.animation'),dict(frame=24,action='click',target='node.hero'),dict(frame=28,action='click',target='animation.play-pause'),dict(frame=33,action='click',target='node.guide'),dict(frame=38,action='click',target='animation.play-pause')]
    animation=launch(executable,root,'animation',(1920,1080),1,actions,capture=True)
    tracks=animation['animationTimelines']
    assert tracks['hero']['playing'] and tracks['guide']['playing']
    assert tracks['hero']['time']>tracks['guide']['time']>0
    assert not animation['dirty'] and animation['undoCount']==0
    retained_workflows(executable,root)
    (root/'summary.json').write_text(json.dumps(dict(assistance=True,scripts=True,animation=tracks),indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();run(args.executable.resolve(),args.output.resolve())
