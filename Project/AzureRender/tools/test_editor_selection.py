"""Check shared selection rules through actual filtered Outliner and viewport input."""
import argparse
import json
import shutil
import time
from pathlib import Path
from create_playable_project import create
from editor_test_host import EditorHost
from test_editor_workspace import launch


def reveal_during_search(executable, project_file, output):
    def visible_nodes(workspace):
        return {widget.removeprefix('node.') for widget in workspace['widgets']
                if widget.startswith('node.')}

    with EditorHost(executable, project_file, output) as host:
        host.click('outliner.search')
        host.call('input', event={'action': 'text', 'text': 'Range'})
        host.frames()
        before = host.workspace()
        assert visible_nodes(before) == {'range-a', 'range-b', 'range-c'}
        host.click('node.range-a')
        host.click('outliner.create')
        host.click('preset.empty')
        created = host.workspace()
        assert created['nodeCount'] == before['nodeCount'] + 1
        created_id, = created['selection']
        assert created_id in visible_nodes(created)
        assert 'outliner.return-to-search' in created['widgets'], 'Creation must offer to return to the search'
        host.screenshot('outliner-return-' + output.parent.name)
        host.click('outliner.return-to-search')
        restored = host.workspace()
        assert visible_nodes(restored) == visible_nodes(before)
        assert restored['selection'] == [created_id]
        assert restored['undoCount'] == created['undoCount']
        assert 'outliner.return-to-search' not in restored['widgets']
        host.screenshot('outliner-search-restored-' + output.parent.name)

        host.click('menu.Edit')
        host.click('menu.Undo')
        assert host.workspace()['nodeCount'] == before['nodeCount']
        host.click('menu.Edit')
        host.click('menu.Redo')
        replayed = host.workspace()
        assert replayed['selection'] == [created_id]
        assert 'outliner.return-to-search' in replayed['widgets']
        host.click('outliner.search')
        host.call('input', event={'action': 'text', 'text': 'range-b'})
        host.frames()
        replaced = host.workspace()
        assert visible_nodes(replaced) == {'range-b'}
        assert 'outliner.return-to-search' not in replaced['widgets']


def run(executable, output, workflow):
    output.mkdir(parents=True)
    project = output / 'game'
    if workflow == 'exploration':
        create(project)
    else:
        shutil.copytree(Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector', project,
                        ignore=shutil.ignore_patterns('.azure'))
    descriptor = json.loads((project / 'project.azureproject').read_text(encoding='utf-8'))
    level = project / 'assets' / descriptor['startupScene'].removeprefix('assets:/')
    document = json.loads(level.read_text(encoding='utf-8'))
    for index, label in enumerate('abc'):
        document['nodes'].append(dict(id='range-' + label, name='Range ' + label.upper(), components={
            'azure.transform': dict(type='azure.transform', version=1, data=dict(
                translation=[10 + index, 0, 0], rotation=[0, 0, 0], scale=[1, 1, 1]))}))
    level.write_text(json.dumps(document), encoding='utf-8')
    saved = level.read_bytes()
    reveal_during_search(executable, project / 'project.azureproject', output / 'reveal-search')
    assert level.read_bytes() == saved, 'Returning to search must preserve the saved scene'
    events = [dict(frame=25, action='click', target='outliner.search'),
              dict(frame=29, action='text', text='Range'),
              dict(frame=34, action='click', target='node.range-a'),
              dict(frame=39, action='click', target='node.range-c', ctrl=True)]
    selected = launch(executable, output, 'filtered-ctrl', (1280, 720), 1, events, capture=False, close_policy='discard')
    assert selected['selection'] == ['range-a', 'range-c'], 'Filtering must preserve Ctrl toggle semantics'
    ranged = launch(executable, output, 'filtered-range', (1280, 720), 1,
                    events + [dict(frame=44, action='click', target='node.range-b', shift=True)],
                    capture=False, close_policy='discard')
    assert set(ranged['selection']) == {'range-b', 'range-c'}, 'Shift range follows visible filtered order'
    assert ranged['history'][-1]['selected'] == 'range-b', 'Clicked range endpoint must stay active'
    # Empty viewport fixture has no geometry, so blank selection is unambiguous.
    blank_document = json.loads(level.read_text(encoding='utf-8'))
    blank_document['nodes'] = [node for node in blank_document['nodes'] if node['id'].startswith('range-')]
    blank_document.pop('prefabs', None)
    blank_document['lights'] = []
    level.write_text(json.dumps(blank_document), encoding='utf-8')
    saved = level.read_bytes()
    image = selected['image']
    blank = [-image['width'] / 2 + 4, -image['height'] / 2 + 4]
    additive = launch(executable, output, 'ctrl-blank', (1280, 720), 1,
                      events + [dict(frame=44, action='click', target='viewport.image', offset=blank, ctrl=True)],
                      capture=False, close_policy='discard')
    assert additive['selection'] == selected['selection'], 'Ctrl blank click preserves selection'
    cleared = launch(executable, output, 'blank', (1280, 720), 1,
                     events + [dict(frame=44, action='click', target='viewport.image', offset=blank)],
                     capture=False, close_policy='discard')
    assert cleared['selection'] == [], 'Blank viewport click clears selection'
    assert level.read_bytes() == saved and not cleared['dirty'], 'Selection preserves authored scene bytes'
    summary = dict(workflow=workflow, passed=True, restoredSearchAfterCreate=True, filteredCtrl=True, filteredRange=True,
                   activeEndpoint=True, blankClear=True, ctrlBlankPreserves=True, sceneBytes=True)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    folder = args.output.resolve() / ('run-' + str(time.time_ns()))
    summaries = [run(args.executable.resolve(), folder / workflow, workflow)
                 for workflow in ('exploration', 'inspector')]
    (folder / 'summary.json').write_text(json.dumps(summaries, indent=2), encoding='utf-8')
    print(json.dumps(summaries))
