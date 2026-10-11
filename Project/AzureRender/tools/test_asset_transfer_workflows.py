"""Replay asset and subtree export through the production Dear ImGui forms."""
import argparse
import json
from pathlib import Path
import tempfile
from create_playable_project import create
from test_editor_workspace import launch


def run(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    create(root / 'game')
    baseline = launch(executable, root, 'baseline', (1920, 1080), 1)
    model = next(row['id'] for row in baseline['visibleAssets'] if row['type'] == 'model')
    output = root / '交换 🎨 模型'
    actions = [dict(frame=25, action='click', target='asset.' + model),
               dict(frame=32, action='click', target='assets.import-create'),
               dict(frame=40, action='click', target='assets.export'),
               dict(frame=48, action='click', target='export.path'),
               dict(frame=51, action='key', key='A', down=True, ctrl=True),
               dict(frame=52, action='key', key='A', down=False, ctrl=True),
               dict(frame=53, action='key', key='A', down=False, ctrl=False),
               dict(frame=56, action='text', text=str(output)),
               dict(frame=64, action='click', target='export.plan'),
               dict(frame=76, action='click', target='export.start')]
    exported = launch(executable, root, 'asset-export', (1920, 1080), 1, actions)
    tasks = [task for task in exported['tasks'] if task['source'] == 'asset.export']
    assert len(tasks) == 1 and tasks[0]['state'] == 'completed', tasks
    assert Path(tasks[0]['result']['output']).is_file()
    assert Path(tasks[0]['result']['output']).parent.resolve() == output.resolve()
    assert not exported['dirty'] and exported['feedback'] == []
    subtree = root / '场景 子树'
    actions = [dict(frame=25, action='click', target=next(key for key in baseline['widgets'] if key.startswith('node.') and not key.endswith('.expand'))),
               dict(frame=32, action='click', target='outliner.actions'),
               dict(frame=40, action='click', target='outliner.export'),
               dict(frame=48, action='click', target='export.path'),
               dict(frame=51, action='key', key='A', down=True, ctrl=True),
               dict(frame=52, action='key', key='A', down=False, ctrl=True),
               dict(frame=53, action='key', key='A', down=False, ctrl=False),
               dict(frame=56, action='text', text=str(subtree)),
               dict(frame=64, action='click', target='export.plan'),
               dict(frame=76, action='click', target='export.start')]
    exported = launch(executable, root, 'subtree-export', (1920, 1080), 1, actions)
    tasks = [task for task in exported['tasks'] if task['source'] == 'asset.export']
    assert len(tasks) == 1 and tasks[0]['state'] == 'completed', tasks
    assert Path(tasks[0]['result']['output']).is_file()
    assert Path(tasks[0]['result']['output']).parent.resolve() == subtree.resolve()
    assert not exported['dirty'] and exported['feedback'] == []
    summary = dict(passed=True, productionUiWorkflows=2, unicodeDestinations=True,
                   publicTasks=True, savedDocumentPreserved=True, manualReview='P8')
    (root / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure-transfer-ui-') as temporary:
        run(args.executable.resolve(), args.output.resolve() if args.output else Path(temporary))
