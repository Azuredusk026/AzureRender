"""Exercise search recovery and property layout through production input."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from test_editor_workspace import launch
from create_playable_project import create


def run(executable, root):
    root = root / ('run-' + uuid.uuid4().hex[:8])
    root.mkdir(parents=True)
    results = []
    for workflow in ('scene_inspector', 'exploration'):
        work = root / workflow
        if workflow == 'exploration':
            create(work / 'game')
        else:
            shutil.copytree(Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector', work / 'game', ignore=shutil.ignore_patterns('.azure'))
        search = [dict(frame=25, action='click', target='outliner.search'),
                  dict(frame=29, action='text', text='没有匹配的对象_UI6')]
        empty = launch(executable, work, 'empty-search', (1280,720), 1, search, capture=True)
        assert 'outliner.clear-search' in empty['widgets'], 'Empty hierarchy search must provide an actionable recovery control'
        assert not any(k.startswith('node.') for k in empty['widgets']), 'Unmatched search must hide node rows'
        restored = launch(executable, work, 'restored-search', (1280,720), 1, search + [dict(frame=35, action='click', target='outliner.clear-search')], capture=False)
        assert any(k.startswith('node.') for k in restored['widgets']) and not restored['dirty']
        node = 'observer' if workflow == 'scene_inspector' else 'hero:body'
        for name,size,dpi in [('normal',(1920,1080),1),('scaled',(1280,720),1.5)]:
            data = launch(executable, work, 'properties-'+name, size, dpi, [dict(frame=25, action='click', target='node.'+node)], capture=True)
            rect = data['panels']['inspector']['rect']
            for axis in range(3):
                value = data['widgets']['axis.value.azure.transform.translation.'+str(axis)]
                assert rect[0] <= value[0] and value[0]+value[2] <= rect[0]+rect[2], ('Property axis must fit the inspector', rect, value)
            assert data['feedback']==[] and not data['dirty']
        focus = launch(executable, work, 'search-focus', (1280,720), 1, search + [
            dict(frame=34,action='key',key='Delete',down=True),dict(frame=35,action='key',key='Delete',down=False)], capture=False)
        assert focus['nodeCount']==empty['nodeCount'] and not focus['dirty'], 'Search focus must protect scene editing shortcuts'
        results.append(dict(workflow=workflow,searchRecovery=True,axisBounds=True,focusProtection=True))
    result=dict(passed=True,workflows=results,input='production ImGui replay',physicalInput='not performed')
    (root/'summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result))


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('--executable',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    args=p.parse_args()
    run(args.executable.resolve(),args.output.resolve())
