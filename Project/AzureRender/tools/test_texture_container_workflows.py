"""Exercise descriptor-driven compression choices through production ImGui."""
import argparse
import json
from pathlib import Path
import shutil
import tempfile
from create_playable_project import create
from test_editor_workspace import launch


def run(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    create(root / 'game')
    source = Path(__file__).resolve().parents[1]
    shutil.copy2(source / 'docs/acceptance/g19/evidence/2026-10-12/image-fixtures/alpha.png', root / 'game/assets/swatch.png')
    baseline = launch(executable, root, 'baseline', (1920, 1080), 1)
    assert 'assets.search' in baseline['widgets']
    image = json.loads((root / 'game/assets/swatch.png.azmeta').read_text(encoding='utf-8'))['id']
    results = []
    for format_id, compression in [('dds', 'bc7'), ('ktx2', 'basis')]:
        output = root / ('压缩纹理 🎨 ' + format_id)
        actions = [dict(frame=10, action='click', target='assets.search'),
                   dict(frame=13, action='text', text='swatch'),
                   dict(frame=25, action='click', target='asset.' + image),
                   dict(frame=32, action='click', target='assets.import-create'),
                   dict(frame=40, action='click', target='assets.export'),
                   dict(frame=48, action='click', target='export.format'),
                   dict(frame=51, action='click', target='export.format.' + format_id),
                   dict(frame=56, action='click', target='export.setting.compression'),
                   dict(frame=59, action='click', target='export.setting.compression.' + compression),
                   dict(frame=64, action='click', target='export.path'),
                   dict(frame=67, action='key', key='A', down=True, ctrl=True),
                   dict(frame=68, action='key', key='A', down=False, ctrl=True),
                   dict(frame=69, action='key', key='A', down=False, ctrl=False),
                   dict(frame=72, action='text', text=str(output)),
                   dict(frame=80, action='click', target='export.plan'),
                   dict(frame=92, action='click', target='export.start')]
        data = launch(executable, root, format_id, (1920, 1080), 1, actions)
        tasks = [task for task in data['tasks'] if task['source'] == 'asset.export']
        assert len(tasks) == 1 and tasks[0]['state'] == 'completed', tasks
        result = tasks[0]['result']
        assert result['compression'] == compression, result
        assert result['losses'] and Path(result['output']).parent.resolve() == output.resolve(), result
        assert not data['dirty'] and not data['feedback'], data['feedback']
        results.append(dict(format=format_id, compression=compression, result=result))
    summary = dict(passed=True, productionUiWorkflows=2, unicodeDestinations=True, results=results, manualReview='P8')
    (root / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure-container-ui-') as temporary:
        run(args.executable.resolve(), args.output.resolve() if args.output else Path(temporary))
